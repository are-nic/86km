#include "DHT.h"                      // библиотека датчика
#include "U8glib.h"                   // библиотека экрана
#include <EtherCard.h>                // библиотека ethernet модуля
#include <EEPROM.h>                   // библиотека энергонезависимой памяти

#define PIN_VOLT_DISEL A1             // пин А1 для измерения напряжения питания дизельгенератора
#define PIN_VOLT_SHELTER A4           // вход акб шельтер 24V
#define PIN_VOLT_SIGNAL A2            // пин А2 для измерения напряжения "сигнализация"
#define DHTPIN_SHELTER 2              // вывод, к которому подключается датчик шельтера
#define DHTPIN_STREET 3               // вывод, к которому подключается датчик улицы
#define DHTPIN_SDMO 4                 // вывод, к которому подключается датчик sdmo
  

#define DHTTYPE DHT22                 // DHT_22 (AM2302)

DHT dht_shel(DHTPIN_SHELTER, DHTTYPE);
DHT dht_str(DHTPIN_STREET, DHTTYPE);
DHT dht_sdmo(DHTPIN_SDMO, DHTTYPE);
U8GLIB_ST7920_128X64_1X u8g(13, 11, 10);  // Создаём объект u8g для работы с дисплеем, указывая номер вывода CS для аппаратной шины SPI

int errors = 0;                       // количество ошибок при считывании температуры

float vd_Value_out;
float vc_Value_out;
float vs_Value_out;
float vd;
float vc;
float vs;
float r1 = 150000.0;
float r2 = 50000.0;

// переменные для записи значений датчиков и отрисовки на веб-страницу
char t_shel_str[7];
char t_sdmo_str[7];
char t_street[7];
char vd_str[7];
char vc_str[7];
char vs_str[7];
//---------------------------------------------------------------------------------

static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };    // mac-адрес должен быть уникальным для данной сети
static byte myip[] = { 192,168,1,203 };                     // ip-адрес, по которому обращаться для вывода информации в браузере

byte Ethernet::buffer[3000];    // задаем размер буфера отправки и получения по tcp/ip
BufferFiller bfill;             // указатель буфера данных Ethernet

//----------------------------------------------------------------------------------

void(* resetFunc) (void) = 0; //объявляем функцию reset с адресом 0

void setup() {
  pinMode(PIN_VOLT_DISEL, INPUT);             // настраиваем 1 аналоговый вход для вольтажа дизеля
  pinMode(PIN_VOLT_SHELTER, INPUT);           // наприяжение акб шельтер 24 v
  pinMode(PIN_VOLT_SIGNAL, INPUT);            // настраиваем 2 аналоговый вход для вольтажа сигнализация
    
  dht_str.begin();
  dht_sdmo.begin();
  dht_shel.begin();

  Serial.begin(9600);                         // подключаем монитор порта

  u8g.setFont(        u8g_font_7x14        );                 // выбираем шрифт для вывода на LED-экране
    
  //----------------------------------------------------------------------------------
  // Изменить CS- pin, если он не используется по умолчанию (8 пин)
  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0)        // Если не получены какие-либо данные для инициализации соединения с контроллером
    Serial.println(F("Failed to access Ethernet controller"));    // Ошибка доступа к Ethernet контроллеру                             
  ether.staticSetup(myip);                  // устанавливаем текущий статический ip контроллеру     
  //----------------------------------------------------------------------------------
}

void loop() {
  /* ТЕМПЕРАТУРА */
  delay(300);                                  // время опроса датчиков
  float t_str = dht_str.readTemperature();    // считывание температуры на улице
  float t_sdmo = dht_sdmo.readTemperature();  // считывание температуры в дизельной
  float t_shel = dht_shel.readTemperature();  // считывание температуры в шельтере

  /* ВОЛЬТАЖ */
  vd_Value_out = float(analogRead(PIN_VOLT_DISEL))    / 194;
  vc_Value_out = float(analogRead(PIN_VOLT_SIGNAL))   / 184;
  vs_Value_out = float(analogRead(PIN_VOLT_SHELTER))  / 112.8;
  vd = vd_Value_out / (r2/(r1+r2));  
  vc = vc_Value_out / (r2/(r1+r2));
  vs = vs_Value_out / (r2/(r1+r2));

  // *************************ВЫВОД ИНФОРМАЦИИ НА WEB-СЕРВЕР (веб-страницу с параметрами)************************************
  // переводим данные датчиков из цифрового вида в строковый для отрисовки на Веб-странице
  dtostrf(t_shel, 5, 2, t_shel_str);
  dtostrf(t_sdmo, 5, 2, t_sdmo_str);
  dtostrf(t_str, 5, 2, t_street);
  dtostrf(vd, 5, 2, vd_str);
  dtostrf(vs, 5, 2, vs_str);
  dtostrf(vc, 5, 2, vc_str);
  
  word len = ether.packetReceive();   // принимает новый входящий пакет из сети
  word pos = ether.packetLoop(len);   // отвечает на определенные входящие сообщения, в том числе на «ping» ( эхо-запрос ICMP)
  
  if(pos) {                   // если валидные данные tcp получены
  
    if(strstr((char *)Ethernet::buffer + pos, "reset") != 0)  // перезагрузить адруино при нажатии на кнопку "Перезагрузить адруино"
    {
      resetFunc();  
    }
           
    bfill = ether.tcpOffset();  // указатель буфера данных Ethernet
    
    // HTML-код страницы 
    bfill.emit_p(PSTR(
      "HTTP/1.0 200 OK\r\n"
      "Content-Type: text/html\r\nPragma: no-cache\r\n\r\n"
      "<meta http-equiv='refresh' content='5' charset='utf-8'/>"    // браузер будет обновлять страницу через каждые 5 секунд
      "<html><head><title>Удаленный контроль ПРС-86км</title></head>"
      "<html><body>"
      "<H1>Удаленный контроль ПРС-86км</H1>"
      "<form method=\"POST\">"));
    bfill.emit_p(PSTR(
      "<table border=\"1\">"
      "<tr><td>Температура в Шельтере:</td><td> $S &deg;C </td></tr>"
      "<tr><td>Температура в Дизельной:</td><td> $S &deg;C </td></tr>" 
      "<tr><td>Температура наружняя:</td><td> $S &deg;C </td></tr>"
      "<tr><td>Напряжение Дизель:</td><td> $S В </div></td></tr>"
      "<tr><td>Напряжение АКБ 24В шельтер:</td><td> $S В </div></td></tr>"
      "<tr><td>Напряжение Сигнализация:</td><td> $S В </div></td></tr>"
      "</table>"), t_shel_str, t_sdmo_str, t_street, vd_str, vs_str, vc_str);     
    bfill.emit_p(PSTR("<button name=\"reset\">Перезагрузить Arduino</button></div>"));
    bfill.emit_p(PSTR("</form></body></html>"));
    
    ether.httpServerReply(bfill.position());  // отправляем данные веб-страницы
  }   
  Serial.println(t_shel);
   //ВЫВОД НА ЭКРАН ДАННЫХ ТЕМПЕРАТУРЫ И НАПРЯЖЕНИЯ
  u8g.firstPage();                        // Всё что выводится на дисплей указывается в цикле: u8g.firstPage(); do{ ... команды ... }while(u8g.nextPage());
  do {
   
 // температура шельтера
    String tIII = "SH-R t" + String(t_shel) ;
    u8g.drawStr(0, 15, tIII.c_str());    // вывод температуры дэс на дисплей

     // температура cdmo
    String tsdmo = "SDMO t" + String(t_sdmo) ;
    u8g.drawStr (0, 30, tsdmo.c_str ());    // вывод температуры шельтера на дисплей

    // температура наружняя
    String tH = "AIR    t " + String(t_str);
    u8g.drawStr(0, 63, tH.c_str());     // вывод температуры наружней

    // напряжение дизель
    String v1 = " V" + String(vd);
    u8g.drawStr(80, 30, v1.c_str());     // вывод напряжения в дизельной на дисплей

      // напряжение акб шельтер
    String v3 = " V" + String(vs);
    u8g.drawStr(80, 15, v3.c_str());     // вывод напряжения в шельтер 24 v на дисплей

    // напряжение дизель
    String v2 = "SIGNAL  V " + String(vc);
    u8g.drawStr(0, 47, v2.c_str());     // вывод напряжения сигнализация
  }
  while (u8g.nextPage());
}

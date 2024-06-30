/*  Realizdo por gurues@3DO 
-----------------------------------------------------------------------------------------------------------
-------------------------------         CLOCK MESSENGER         -------------------------------------------
-----------------------------------------------------------------------------------------------------------

  Mediante un Wemos D1 mini y una matriz led MAX-7219 FC-16 se realiza un dispositivo que mostrará la hora y
  mensajes básicos repetitivos de fecha y tiempo de la ciudad a elección. También se crea una app en Blynk
  para controlar el dispositivo así como para enviar mensajes instantaneos, repetitivos o programados. Solo
  con 5 DataStreams (restricción de Blynk)

***********************************************************************************************************
-- CONEXIÓN HARDWARE (Wemos D1 mini o ESP8266 <--> FC16):
-- 3V <--> VCC, GND <--> GND, D7 <--> DIN, D8 <--> CS, D5 <--> CLK
-- CONEXIÓN HARDWARE (Wemos D1 mini o ESP8266 <--> BUZZER):
-- D1 <--> - , GND, GND
-- Otra forma de conexión es mediante un transistor NPN
-- D1 Wemos/ESP <--> R1= 220 ohm, R1 <--> Base NPN, Emisor NPN <--> GND Wemos/ESP, Colector NPN <--> - Buzzer
-- + Buzzer <--> 5v Wemos/ESP, + diodo <--> - Buzzer, - diodo <--> + Buzzer
-- CONEXIÓN HARDWARE (Wemos D1 mini o ESP8266 <--> RTC -> DS3231):
-- D2, D3 <--> RTC -> DS3231 (si no disponemos conexión permanente a internet)
***********************************************************************************************************

*/

// Actualizar con tus datos de BLYNK
#define BLYNK_TEMPLATE_ID "XXXXXXXXXXXXXXXXXX"
#define BLYNK_TEMPLATE_NAME "ClockMessenger"
#define BLYNK_AUTH_TOKEN "XXXXXXXXXXXXXXXXXXX"

#define BLYNK_FIRMWARE_VERSION  "1.0.0"

// Comentar para deshabilitar el Debug de Blynk
//#define BLYNK_PRINT Serial        // Defines the object that is used for printing
//#define BLYNK_DEBUG               // Optional, this enables more detailed prints

#include <Arduino.h>
#include <configuracion.h>          // Configuración de usuario
#include <LedControlSPIESP8266.h>   // Control display led MAX7219 FC-16
#include <FC16.h>                   // Control display led MAX7219 FC-16
#include <WifiUDP.h>                // sincro servidor NTP
#include <TimeLib.h>                // sincro servidor NTP
#include <Ticker.h>                 // Temporizador funciones asincronas
#include <ArduinoOTA.h>             // Actualización por OTA
#include <ESP8266mDNS.h>            // Actualización por OTA
#include <BlynkSimpleEsp8266.h>     // Control externo mediante app
#include <ESP8266HTTPClient.h>      // openweathermap
#include <ArduinoJson.h>            // openweathermap
#include <DNSServer.h>              // Genera portal cautivo para configurar la wifi y variables
#include <ESP8266WebServer.h>       // Genera portal cautivo para configurar la wifi y variables
#include <WiFiManager.h>            // Genera portal cautivo para configurar la wifi y variables
#include <EasyBuzzer.h>             // Buzzer activo 
#ifdef ___RTC___
  #include <Wire.h>                   // RTC clock DS1307 y DS3231
  #include <RTClib.h>                 // RTC clock DS1307 y DS3231
#endif

// Configura el cliente NTP UDP
WiFiUDP ntpUDP;
unsigned int localPort = 8888;      // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48;     // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

// NTP Servers:
//static const char ntpServerName[] = "pool.ntp.org";
//static const char ntpServerName[] = "es.pool.ntp.org";
static const char ntpServerName[] = "ntp.roa.es";

time_t prevDisplay = 0; // variable con la fecha Y hora a mostrar
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
bool sincronizado = false;   // variable de control primera sincronización

// Cliente web
HTTPClient clienteHttp;

// Cliente WIFI
WiFiClient clienteWIFI;

//Configurar LED MAC7219
const int csPin = D8;						          // CS pin used to connect FC16
const int displayCount = 4;			          // Number of displays; usually 4 or 8
FC16 display = FC16(csPin, displayCount);	// Define display parameters

//Configurar Buzzer
const int BuzzerPin = D1;                 // Pin del Buzzer activo de sonido

#ifdef ___RTC___
  //Configurar RTC DS1307 y DS3231
  const int PIN_SDA = D3;                   // Pin SCA I2C -> pin D3 shield D1 RTC1307
  const int PIN_SCL = D2;                   // Pin SCL I2C -> pin D2 shield D2 RTC1307

  RTC_DS3231 rtc;
  DateTime ahora;                           // hora RTC
#endif

//Variables grabales de control del programa
//Week Days
String weekDays[7] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};
//Month names
String months[12]={"Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"};

int currentHour, currentMinute, currentSecond;  // variables del mensaje clock
String Mensajes, Men_rep, Men_pro;              // variables Mensajes instantaneo, repetitivo y programado
bool message = false;                           // control de mensajes
bool control_instant = false;                   // control instantaneo
int sincro_rep = 0;                             // sincronizo el ticker de mensajes repetitivos
int control_rep = 0;                            // control de mensajes repetitivos app Blink
int rep_min = 60;                               // tiempo en segundos entre mensajes repetitivos app Blink
int control_prog = 0;                           // control de mensajes programados app Blink
int prog_hora = 14;                             // hora del mensaje programado y alarma 1 = 14:28
int prog_min = 28;                              // minuto del mensaje programado y alarma 1
int prog_hora2 = 16;                            // hora alarma 2 = 16:02
int prog_min2 = 2;                              // minuto alarma 2
int delayMensajes = 120;                        // tiempo de muestreo de mensajes rodantes
int memdelayMensajes = delayMensajes;           // memoria tiempo de muestreo de mensajes rodantes
int timeZone = 10;                              // Adelanto horario respecto hora solar se actualiza en la funcion obtenerDatosBasicos()
int sonido = 0;                                 // control de aviso por sonido de la llegada de mensajes
int alarma = 0;                                 // alarma programada
float offset = -4;                              // offset de temperatura
bool conect_wifi = false;                       // estado de conexión del clock messenger
String overTheAirURL = "";                      // URL para OTA desde Blynk

// Programadores de eventos Ticker
Ticker ticker_Datos, ticker_retardo_message, ticker_Clock,
        ticker_programador, ticker_mensaje_rep, ticker_mensaje_prog, 
        ticker_delay_60, ticker_delay_prog_60; 


/////////////////////////////////////////////////////////////////////////////////
//           FUNCIONES
/////////////////////////////////////////////////////////////////////////////////

// Control retraso mensajes con app Blink
BLYNK_WRITE(V0){                //-> se auto sicronizar Blynk app
  delayMensajes = param.asInt();
  memdelayMensajes = delayMensajes;
  #ifdef ___DEBUG___
    Serial.print("delayMensajes = ");
    Serial.println(delayMensajes);
  #endif
}

// Terminal - Control de Clock Messenger con app Blink 
WidgetTerminal terminal(V1);  // No se auto sicronizar Blynk app
BLYNK_WRITE(V1){
  Blynk.syncVirtual(V0);
  String Men = param.asStr();

  // Muestra los comandos de control de la app Blynk
  if (Men == "/HELP/"){
    terminal.clear();
    terminal.println("");
    terminal.println("/HELP/: Muestra la Ayuda de comandos");
    terminal.println("Mensajes Instantaneos comienzan por: @ ");
    terminal.println("Mensajes Repetitivos comienzan por: R@ ");
    terminal.println("Mensajes Programados comienzan por: P@ ");
    terminal.println("/MENSAJES/: Muestra los Mensajes Repetitivos y Programados configurados");
    terminal.println("/parametro=: El numero después del = se configura como valor del parametro (ej: /offset=-3.5)");
    terminal.println("/parametro/: Muestra el valor actual del parametro de control (ej: /offset/)");
    terminal.println("/rep_min=: Configura los mensajes se repiten cada x minutos");
    terminal.println("/prog_h:min=: Configura hora del mensaje programado o de la alarma");
    terminal.println("/prog_h:min_2=: Configura hora de la alarma 2");
    terminal.println("/sonido=: Activa/Desactiva (1/0) mensajes con alerta sonora");
    terminal.println("/alarma=: Activada/Desactivada (1/0) Alarma sonora en Clock_Messenger");
    #ifdef ___RTC___
      terminal.println("/offset=: Configura el offset de temperatura (ej: /OFFSET=-3.5)");
    #endif
    terminal.println("/ESTADO/: Muestra el estado de los parametros de control Clock_Messenger");
    terminal.println("/CLEAR/: Limpia Terminal y vacia mensajes Repetitivos y Programados");
    terminal.flush();
  }

  // Se evalua el mensaje a enviar: instantaneo, repetitivo, programado
  if (Men.substring(0, 1) == "@"){  // instantaneo
    Mensajes = Men.substring(1, Men.length());
    message = true;
    control_instant = true;
  }
  if (Men.substring(0, 2) == "R@"){  // repetitivo
    Blynk.virtualWrite(V2, 0);
    control_rep = 0;
    Men_rep = Men.substring(2, Men.length());
    control_instant = false;
  }
  if (Men.substring(0, 2) == "P@"){  // programado
    Blynk.virtualWrite(V3, 0);
    control_prog = 0;
    Men_pro = Men.substring(2, Men.length());
    control_instant = false;
  }
  
  // Información de los mensajes configurados
  if (Men == "/MENSAJES/"){
    terminal.println("Mensaje Repetitivo: R@ ");
    terminal.println(Men_rep);
    terminal.println("Mensajes Programado: P@ ");
    terminal.println(Men_pro);
    terminal.flush();
  }

  // Control de mensajes con repetición
  if (Men == "/rep_min/"){
      terminal.print("Los mensajes se repiten cada ");
      terminal.print(rep_min/60);
      terminal.println(" minutos");
      terminal.flush();
  }
  if (Men.substring(0, 9) == "/rep_min="){
    rep_min = Men.substring(9, Men.length()).toFloat();
    rep_min = rep_min*60;
    terminal.println(" ");
    terminal.print("Actualizado rep_min a ");
    terminal.print(rep_min/60);
    terminal.println(" minutos");
    terminal.flush();
  }

  // Control alerta sonora mensajes 
  if (Men == "/sonido/"){
    if (sonido==1){
      terminal.println(" ");
      terminal.println("Activado mensajes con alerta sonora en Clock_Messenger");
      terminal.flush();
    }
    if (sonido==0){
      terminal.println(" ");
      terminal.println("Desactivados mensajes con alerta sonora en Clock_Messenger");
      terminal.flush();
    }
  }
  if (Men.substring(0, 8) == "/sonido="){
    sonido = Men.substring(8, Men.length()).toFloat();
    if (sonido==1){
      terminal.println(" ");
      terminal.println("Activado mensajes con alerta sonora en Clock_Messenger");
      terminal.flush();
    }
    if (sonido==0){
      terminal.println(" ");
      terminal.println("Desactivados mensajes con alerta sonora en Clock_Messenger");
      terminal.flush();
    }
  }

  // Control alarma
  if (Men == "/alarma/"){
    if (alarma==1){
      terminal.println(" ");
      terminal.println("Activada Alarma sonora en Clock_Messenger");
      terminal.flush();
    }
    if (alarma==0){
      terminal.println(" ");
      terminal.println("Desactivada Alarma Sonora en Clock_Messenger");
      terminal.flush();
    }
  }
  if (Men.substring(0, 8) == "/alarma="){
    alarma = Men.substring(8, Men.length()).toFloat();
    if (alarma==1){
      terminal.println(" ");
      terminal.println("Activada Alarma sonora en Clock_Messenger");
      terminal.flush();
    }
    if (alarma==0){
      ticker_delay_60.detach();
      terminal.println(" ");
      terminal.println("Desactivada Alarma Sonora en Clock_Messenger");
      terminal.flush();
    }
  }

  // Control mensajes programados y alarma 1
  if (Men == "/prog_h:min/"){
    terminal.print("Hora Programada ");
    terminal.print(prog_hora);
    terminal.print(":");
    if(prog_min<10)
    terminal.print("0");
    terminal.print(prog_min);
    terminal.println("h");
    terminal.flush();
  }
  if (Men.substring(0, 12) == "/prog_h:min="){
    prog_hora = Men.substring(12, 15).toInt();
    prog_min = Men.substring(15, Men.length()).toInt();
    terminal.println(" ");
    terminal.print("Hora Programada: ");
    terminal.print(prog_hora);
    terminal.print(":");
    if(prog_min<10)
      terminal.print("0");
    terminal.print(prog_min);
    terminal.println("h");
    terminal.flush();
  }

  // Control alarma 2
  if (Men == "/prog_h:min_2/"){
    terminal.print("Hora Programada alarma 2: ");
    terminal.print(prog_hora2);
    terminal.print(":");
    if(prog_min2<10)
      terminal.print("0");
    terminal.print(prog_min2);
    terminal.println("h");
    terminal.flush();
  }
  if (Men.substring(0, 14) == "/prog_h:min_2="){
    prog_hora2 = Men.substring(14, 17).toInt();
    prog_min2 = Men.substring(17, Men.length()).toInt();
    terminal.println(" ");
    terminal.print("Hora Programada Alarma 2: ");
    terminal.print(prog_hora2);
    terminal.print(":");
    if(prog_min<10)
      terminal.print("0");
    terminal.print(prog_min2);
    terminal.println("h");
    terminal.flush();
  }

  // Control ajuste temperatura RTC
  #ifdef ___RTC___
    if (Men == "/offset/"){
      terminal.print("offset de temperatura =");
      terminal.println(offset);
      float temp = rtc.getTemperature() + offset;
      terminal.println("Temperatura = " + String(temp) + "ºC");
      terminal.flush();
    }

    if (Men.substring(0, 8) == "/offset="){
      offset = Men.substring(8, Men.length()).toFloat();
      terminal.println(" ");
      terminal.print("Actualizado offset de temperatura =");
      terminal.println(offset);
      float temp = rtc.getTemperature() + offset;
      terminal.println("Temperatura = " + String(temp) + "ºC");
      terminal.flush();
    }
  #endif

  // Muestra el estado de configuración del Clock_Messenger
  if (Men == "/ESTADO/"){
    //mensajes
    terminal.println("Mensaje Repetitivo: R@ ");
    terminal.println(Men_rep);
    terminal.println("Mensajes Programado: P@ ");
    terminal.println(Men_pro);
    // rep_min - mensajes repetidos
    terminal.print("Los mensajes se repiten cada ");
    terminal.print(rep_min/60);
    terminal.println(" minutos");
    //sonido
    if (sonido==1)
      terminal.println("Activado mensajes con alerta sonora en Clock_Messenger");
    if (sonido==0)
      terminal.println("Desactivados mensajes con alerta sonora en Clock_Messenger");
    //alarma
    if (alarma==1)
      terminal.println("Activada Alarma sonora en Clock_Messenger");
    if (alarma==0)
      terminal.println("Desactivada Alarma Sonora en Clock_Messenger");
    //hora prog y alarma 1
    terminal.print("Hora Programada alarma: ");
    terminal.print(prog_hora);
    terminal.print(":");
    if(prog_min<10)
    terminal.print("0");
    terminal.print(prog_min);
    terminal.println("h");
    //hora prog y alarma 2
    terminal.print("Hora Programada alarma 2: ");
    terminal.print(prog_hora2);
    terminal.print(":");
    if(prog_min2<10)
      terminal.print("0");
    terminal.print(prog_min2);
    terminal.println("h");
    // Offset de temperatura
    #ifdef ___RTC___
      terminal.print("offset de temperatura =");
      terminal.println(offset);
      float temp = rtc.getTemperature() + offset;
      terminal.println("Temperatura = " + String(temp) + "ºC");
    #endif

    terminal.flush();

  }

  // Borrado del Terminal y borrado de mensajes programados y repetitivos
  if (Men == "/CLEAR/"){
    terminal.clear();         
    Men_pro = "";
    Men_rep = "";
    ticker_mensaje_rep.detach();
    ticker_mensaje_prog.detach();
    control_rep = 0;
    control_prog = 0;
    Blynk.virtualWrite(V2,control_rep);
    Blynk.virtualWrite(V3,control_prog);
    Blynk.syncVirtual(V2,V3);
    terminal.println(" ");
    terminal.println("Limpiado Terminal del Clock_Messenger");
    terminal.println("Se han vaciado los mensajes Repetitivos y Programados del Clock_Messenger");
    terminal.flush();
  }

} // Fin de control Terminal V1

// control mensajes repetitivos -> se auto sicronizar Blynk app
BLYNK_WRITE(V2){          
  Blynk.syncVirtual(V0);
  control_rep = param.asInt();
  sincro_rep = 1;
  if ((control_rep == 1)&&(Men_rep=="")){
    terminal.println(" ");
    terminal.println("Mensaje repetitivo vacio, inserta un mensaje repetitivo, R@");
    terminal.flush();
  }
  if ((control_rep == 1)&&(Men_rep!="")){
    terminal.println(" ");
    terminal.println("Activados Mensajes repetitivos en Clock_Messenger");
    terminal.flush();
  }
  if (control_rep == 0){
    terminal.println(" ");
    terminal.println("Desactivados Mensajes repetitivos en Clock_Messenger");
    terminal.flush();
  }
}

// control mensaje programado -> se auto sicronizar Blynk app
BLYNK_WRITE(V3){            
  Blynk.syncVirtual(V0);
  control_prog = param.asInt();
  if ((control_prog == 1)&&(Men_pro=="")){
    terminal.println(" ");
    terminal.println("Mensaje Programado vacio, inserta un mensaje Programado, P@");
    terminal.flush();
  }
  if ((control_prog==1)&&(Men_pro!="")){
    terminal.println(" ");
    terminal.println("Activados Mensajes Programados en Clock_Messenger");
    terminal.flush();
    #ifdef ___DEBUG___
      Serial.println("Activados Mensajes Programados");
    #endif
  }
  if (control_prog==0){
    ticker_delay_prog_60.detach();
    terminal.println(" ");
    terminal.println("Desactivados Mensajes Programados en Clock_Messenger");
    terminal.flush();
    #ifdef ___DEBUG___
      Serial.println("Desactivados Mensajes Programados");
    #endif
  }
}

// Ayuda -> Muestra los comandos de control de la app. Se auto sicronizar Blynk app
BLYNK_WRITE(V4){  
  if (param.asInt() == 1){
    Blynk.syncVirtual(V0);
    terminal.clear();
    terminal.println("");
    terminal.println("/HELP/: Muestra la Ayuda de comandos");
    terminal.println("Mensajes Instantaneos comienzan por: @ ");
    terminal.println("Mensajes Repetitivos comienzan por: R@ ");
    terminal.println("Mensajes Programados comienzan por: P@ ");
    terminal.println("/MENSAJES/: Muestra los Mensajes Repetitivos y Programados configurados");
    terminal.println("/parametro=: El numero después del = se configura como valor del parametro (ej: /offset=-3.5)");
    terminal.println("/parametro/: Muestra el valor actual del parametro de control (ej: /offset/)");
    terminal.println("/rep_min=: Configura los mensajes se repiten cada x minutos");
    terminal.println("/prog_h:min=: Configura hora del mensaje programado o de la alarma");
    terminal.println("/prog_h:min_2=: Configura hora de la alarma 2");
    terminal.println("/sonido=: Activa/Desactiva (1/0) mensajes con alerta sonora");
    terminal.println("/alarma=: Activada/Desactivada (1/0) Alarma sonora en Clock_Messenger");
    #ifdef ___RTC___
      terminal.println("/offset=: Configura el offset de temperatura (ej: /OFFSET=-3.5)");
    #endif
    terminal.println("/ESTADO/: Muestra el estado de los parametros de control Clock_Messenger");
    terminal.println("/CLEAR/: Limpia Terminal y vacia mensajes Repetitivos y Programados");
    terminal.flush();
  }
}

// OTA desde Blynk
BLYNK_WRITE(InternalPinOTA) {
  Serial.println("OTA Started"); 
  overTheAirURL = param.asString();
  Serial.print("overTheAirURL = ");  
  Serial.println(overTheAirURL);  
  WiFiClient my_wifi_client;
  HTTPClient http;
  Blynk.disconnect();
  http.begin(my_wifi_client, overTheAirURL);
  int httpCode = http.GET();
  Serial.print("httpCode = ");  
  Serial.println(httpCode);  
  if (httpCode != HTTP_CODE_OK) {
    return;
  }
  int contentLength = http.getSize();
  Serial.print("contentLength = ");  
  Serial.println(contentLength);   
  if (contentLength <= 0) {
    return; 
  }
  bool canBegin = Update.begin(contentLength);
  Serial.print("canBegin = ");  
  Serial.println(canBegin);    
  if (!canBegin) { 
    return;
  }
  Client& client = http.getStream();
  int written = Update.writeStream(client);
  Serial.print("written = ");  
  Serial.println(written);   
  if (written != contentLength) {
    return;
  }
  if (!Update.end()) {
    return;
  }
  if (!Update.isFinished()) {
    return;
  }
  ESP.restart();
}

// Esta función se ejecutará cada vez que se establezca la conexión Blynk
BLYNK_CONNECTED() {
  // Solicitar al servidor Blynk que vuelva a enviar los últimos valores para todos los pines
  Blynk.syncAll();
}

// Inicio variables app Blynk
void init_app_Blynk(){
  BLYNK_CONNECTED();
  terminal.clear();
  terminal.println("");
  terminal.println("/HELP/: Muestra la Ayuda de comandos");
  terminal.println("Mensajes Instantaneos comienzan por: @ ");
  terminal.println("Mensajes Repetitivos comienzan por: R@ ");
  terminal.println("Mensajes Programados comienzan por: P@ ");
  terminal.println("/MENSAJES/: Muestra los Mensajes Repetitivos y Programados configurados");
  terminal.println("/rep_min=: Configura los mensajes se repiten cada x minutos");
  terminal.println("/prog_h:min=: Configura hora del mensaje programada o de la alarma");
  terminal.println("/sonido=: Activa/Desactiva (0/1) mensajes con alerta sonora");
  terminal.println("/alarma=:Activada/Desactivada (0/1) Alarma sonora en Clock_Messenger");
  terminal.println("/CLEAR/: Limpia Terminal y vacia mensajes Repetitivos y Programados");
  #ifdef ___RTC___
    terminal.println("/OFFSET/: Muestra el offset de temperatura si disponemos de RTC (defecto -3ºC)");
    terminal.println("/OFFSET=: El numero después del = se configura como offset de temperatura (ej: /OFFSET=-3.5)");
  #endif
  terminal.flush();
}

// Configuración del Clock_Messenge mediante portal cautivo: Red WIFI y demas parámetros
void Config_Clock_Messeger(){
  display.clearDisplay();
  display.setText("Si Clock Messenger no se activa automaticamente, conectate a la red WIFI SSID -> Config_Clock -> Pon en tu navegador web -> http://192.168.4.1 -> introduce los datos de tu red WIFI");
  while(!(display.update())){
    delay(scrollDelay );
  }
  WiFiManager wifiManager;
 
  //IP del portal cautivo: Config_Clock_Messeger -> 192.168.4.1
  wifiManager.setAPStaticIPConfig (IPAddress ( 192 , 168 , 4 , 1 ), IPAddress ( 192 , 168 , 1 , 1 ), IPAddress ( 255 , 255 , 255 , 0 ));
  //IP estatica a designar, por defecto 192.168.1.161 dentro de la Red Local donde se conecte el Clock
  #ifdef ip_fija
    wifiManager.setSTAStaticIPConfig(ip, gateway, subnet,dns1);
  #endif
  //conexion wifi
  wifiManager.setConnectRetries(10);                // numero de intentos conexion red wifi anterior
  wifiManager.setTitle("gurues@3DO 2021");          // Titulo del portal cautivo
  if (!wifiManager.autoConnect("Config_Clock")) {   // se intenta conectar y si no se lanza portal
    #ifdef ___DEBUG___
      Serial.println("Fallo en la conexión al portal cautivo de configuración");
    #endif
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  // Reconexión al WIFI tras perdida
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  #ifdef ___DEBUG___
    Serial.println("Conectado a la WIFI con IP Red Local (Config_Clock)");
    WiFi.printDiag(Serial);
    Serial.println("");
  #endif
  
}

//Funciones pra sincronizar con el servidor WEB NTP
time_t getNtpTime(){
  if (WiFi.status() == WL_CONNECTED) {
    IPAddress ntpServerIP; // NTP server's ip address

    while (ntpUDP.parsePacket() > 0){} ; // discard any previously received packets
    #ifdef ___DEBUG___ 
      Serial.println("Transmit NTP Request");
    #endif
      // get a random server from the pool
      WiFi.hostByName(ntpServerName, ntpServerIP);
    #ifdef ___DEBUG___ 
      Serial.print(ntpServerName);
      Serial.print(": ");
      Serial.println(ntpServerIP);
    #endif
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
      int size = ntpUDP.parsePacket();
      if (size >= NTP_PACKET_SIZE) {
        sincronizado=true;
        #ifdef ___DEBUG___ 
          Serial.println("Receive NTP Response");
        #endif
        ntpUDP.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
        unsigned long secsSince1900;
        // convert four bytes starting at location 40 to a long integer
        secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long)packetBuffer[43];
        return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      }
    }
    #ifdef ___DEBUG___ 
      Serial.println("No NTP Response :-( --------- ERROR CONEXION NTP ---------");
    #endif
    return 0; // return 0 if unable to get the time
  }
  else
    return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address){
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  ntpUDP.beginPacket(address, 123); //NTP requests are to port 123
  ntpUDP.write(packetBuffer, NTP_PACKET_SIZE);
  ntpUDP.endPacket();
}

// Obtiene datos de tiempo de openweathermap y fecha del servidor NTP o RTC
void obtenerDatosBasicos() {
  #ifdef ___DEBUG___
    Serial.println("--> obtenerDatosBasicos()");
  #endif
  String weekDay, currentMonthName; 
  int dia, ayo;
  #ifdef ___RTC___
    float temp;
  #endif
  if (WiFi.status() == WL_CONNECTED) {
    // Crear URL para hacer la petición
    String url = hostOpenWeatherMap;
    url += "?id=";
    url += idCiudad;
    url += "&appid=";
    url += apiKey;
    #ifdef ___DEBUG___
      Serial.print("URL petición HTTP: ");
      Serial.println(url);
    #endif
      // Conexión con el servidor y configuración de la petición
    clienteHttp.begin(clienteWIFI, url);
  
    // Envío de petición HTTP al servidor
    int codigoHttp = clienteHttp.GET();
  
    #ifdef ___DEBUG___
      Serial.print("Codigo HTTP: ");
      Serial.println(codigoHttp);
    #endif
  
    // Si todo ha ido bien devolverá un número positivo mayor que cero
    if (codigoHttp > 0) {
      // Si ha encontrado el recurso en el servidor responde un código 200
      if (codigoHttp == HTTP_CODE_OK) {
        #ifdef ___DEBUG___
              Serial.print("Archivo JSON: ");
              Serial.println(clienteHttp.getString());
        #endif
  
        // Parsear archivo JSON
        // Para obtener tamaño del buffer vistiar https://arduinojson.org/v6/assistant/
        const size_t capacity = JSON_ARRAY_SIZE(3) +
                                2 * JSON_OBJECT_SIZE(1) +
                                JSON_OBJECT_SIZE(2) +
                                3 * JSON_OBJECT_SIZE(4) +
                                JSON_OBJECT_SIZE(5) +
                                JSON_OBJECT_SIZE(6) +
                                JSON_OBJECT_SIZE(12) +
                                304;
        DynamicJsonDocument doc(capacity);
  
        // Parsear objeto JSON
        DeserializationError error = deserializeJson(doc, clienteHttp.getString());
        if (error) {
          // Si hay error no se continua
        #ifdef ___DEBUG___
          Serial.print("Fallo al parsear JSON. Error: ");
          Serial.println(error.c_str());
        #endif
          return;
        }

        // Actualizo diferencia horaria
        int timezone = float(doc["timezone"]);
        if (timezone == 0)
          timeZone = 0;
        if (timezone == 3600)
          timeZone = 1;
        if (timezone == 7200)
          timeZone = 2;
        if (timezone == 10800)
          timeZone = 3;

        // Temperatura
        String temperaturaS = String((float(doc["main"]["temp"]))- 273.15) + " C";
        // Humedad
        String humedadS = String(int(doc["main"]["humidity"])) + " %";
        // Viento
        String vientoS = String(float(doc["wind"]["speed"])) + " m/s";
        // Ciudad
        String ciudadS = doc["name"];
        // Tiempo
        String tiempoS = doc["weather"][0]["description"];

        // Asignación de los datos para el volcado de datos a la pantalla
        Mensajes = "-> " + ciudadS + " tiempo " + tiempoS + ", temperatura " + temperaturaS + ", humedad " + humedadS + ", viento " + vientoS + ", ";
      
      #ifdef ___DEBUG___
        Serial.println("Datos OpenWeatherMap");
        Serial.print("Temperatura: ");
        Serial.println(temperaturaS);
        Serial.print("Humedad: ");
        Serial.println(humedadS);
        Serial.print("Viento: ");
        Serial.println(vientoS);
        Serial.print("Ciudad: ");
        Serial.println(ciudadS);
        Serial.print("Diferencia horaria timeZone: ");
        Serial.println(timeZone);
      #endif
      } 
      else {
        #ifdef ___DEBUG___
          Serial.println("Error al recibir petición.");
        #endif
      }
    }

    // Obtenemos la fecha
    weekDay = weekDays[weekday(prevDisplay)-1];  // day of the week for the given time t
    currentMonthName = months[month(prevDisplay)-1]; // the month for the given time t
    dia = day(prevDisplay);
    ayo = year(prevDisplay);
    Mensajes += weekDay + " " + String(dia) + " de " + currentMonthName + " de "+ String(ayo);
    message = true;

  }
  #ifdef ___RTC___
    else{ // Mensaje sin WIFI, datos del RTC
      ahora = rtc.now(); // hora 
      weekDay = weekDays[ahora.dayOfTheWeek()];
      currentMonthName = months[ahora.month()-1]; // the month for the given time t
      dia = ahora.day();
      ayo = ahora.year();
      temp = rtc.getTemperature() + offset;
      Mensajes = "-> Mto. Electrico, temperatura " + String (temp) + ", " + weekDay + ", " + String(dia) + " de " + currentMonthName + " de "+ String(ayo);
      message = true; 

    }  
  #endif

  #ifdef ___DEBUG___ 
    Serial.print("Hour: ");
    Serial.println(currentHour);  
    Serial.print("Minutes: ");
    Serial.println(currentMinute); 
    Serial.print("Seconds: ");
    Serial.println(currentSecond);
    Serial.print("Week Day: ");
    Serial.println(weekDay);    
    Serial.print("Month day: ");
    Serial.println(day(prevDisplay));
    Serial.print("Month: ");
    Serial.println(month(prevDisplay));
    Serial.print("Month name: ");
    Serial.println(currentMonthName);
    Serial.print("Year: ");
    Serial.println(year(prevDisplay));
    #ifdef ___RTC___
      Serial.print("Temperature: ");
      Serial.print(rtc.getTemperature());
      Serial.println(" C");
    #endif
    Serial.println(Mensajes);
    Serial.println("");
  #endif
}

// Actuliza clock mientras no se sincroniza con el servidor WEB NPT
void clock_send(){
    #ifdef ___DEBUG___
      Serial.println("----- clock_send() -----");
    #endif
    display.clearDisplay();		// turn all LED off
    #ifdef ___RTC___
      if (conect_wifi)
        display.setClock(currentHour, currentMinute, currentSecond, offsetdisplay, false);
      else
        display.setClock(currentHour, currentMinute, currentSecond, offsetdisplay, true);
    #else
      display.setClock(currentHour, currentMinute, currentSecond, offsetdisplay, true);
    #endif

    #ifdef ___DEBUG1___
      #ifdef ___RTC___
        Serial.print("Hora Actual RTC: ");
        Serial.print(currentHour); Serial.print(":"); 
        Serial.print(currentMinute); Serial.print(":"); 
        Serial.println(currentSecond);
      #endif
    #endif
}

// Muestra mensaje en display MAX7219 FC-16
void send_message(){
  #ifdef ___DEBUG___
    Serial.print("----- send_message() -----");
  #endif
  display.clearDisplay();		// turn all LED off
  const char *payload = Mensajes.c_str();
  display.setText(payload);
  message = false;
}

// Envio de mensajes repetitivos con app Blink
void programador();
void envio_mensaje_rep(){
  #ifdef ___DEBUG___
    Serial.print("----- envio_mensaje_rep() -----");
  #endif
  Mensajes = Men_rep;
  ticker_Clock.detach();
  ticker_Datos.detach();
  ticker_retardo_message.attach(delayMensajes, programador);
  if (sonido == 1)
      EasyBuzzer.beep(frequency, onDuration, offDuration, beeps, pauseDuration, cycles);
  send_message();
}

// Controla el retorno del programa tras mostrar mensaje y activa las funiones de Clock 
// y datos básicos
void programador(){
  #ifdef ___DEBUG___
    Serial.print("----- programador() -----");
  #endif
  ticker_retardo_message.detach();
  ticker_Clock.attach_ms(1000, clock_send);
  ticker_Datos.attach_scheduled(time_data, obtenerDatosBasicos);
  if (control_instant){
    terminal.println(" ");
    terminal.println("Mensaje instantaneo reproducido en Clock_Messenger");
    terminal.flush();
    control_instant = false;
  }
}

// Controla el tiempo de los mensajes rodandes y después llama a programador para devolver el control 
// al CLock
void retardo_messege(){
  #ifdef ___DEBUG___
    Serial.print("----- retardo_messege() -----");
  #endif
  Blynk.syncVirtual(V0);
  ticker_Clock.detach();
  ticker_Datos.detach();
  // Si el mensaje a enviar es datos basico el mensaje deslizante esta durante 120 seg
  String datos = Mensajes.substring(0, 2);
  if (datos == "->"){
    memdelayMensajes = delayMensajes;
    delayMensajes = retardoBasico;
  }
  else{
    delayMensajes = memdelayMensajes;
    if (sonido == 1)
      EasyBuzzer.beep(frequency, onDuration, offDuration, beeps, pauseDuration, cycles);
  }
  #ifdef ___DEBUG___ 
    Serial.print("Incio Mensajes: ");
    Serial.println(datos);
    Serial.print("delayMensajes: ");
    Serial.println(delayMensajes);
  #endif
  ticker_retardo_message.attach(delayMensajes, programador);
}

// Control de la alarma para que no suene en el mismo minuto
void AlarmaON(){
  alarma =1;
}

// Control de la mensajes programados para que no se reproduzcan en el mismo minuto
void Act_prog(){
  control_prog =1;
}


/////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

    //Inicializar puerto serie
    Serial.begin(115200);

 #ifdef ___DEBUG1___ 
    Serial.println("");
    Serial.println("");
    Serial.println("------ CLOCK MESSENGER INICIALIZANDO ___DEBUG1___ ------");
  #endif 
  #ifdef ___DEBUG___
    Serial.println("");
    Serial.println("");
    Serial.println("------ CLOCK MESSENGER INICIALIZANDO ___DEBUG___ ------");
  #endif

  //Inicio Buzzer
  EasyBuzzer.setPin(BuzzerPin);

  #ifdef ___RTC___
    // Inicio I2C
      Wire.begin(PIN_SDA, PIN_SCL);

    //Inicio RTC DS1307 o DS3231
    if (!rtc.begin()) {
      #ifdef ___DEBUG___
        Serial.println("No encontrado RTC");
      #endif
      while (1);
    }
  #endif
  //Iniciando LED MAX7219
	display.begin();			// turn on display
	display.setIntensity(intensidad);	// set medium brightness
	display.clearDisplay();		// turn all LED off

  // Conectar a wifi //
  Config_Clock_Messeger();

  // Conectar a Blynk //
  #ifdef ___DEBUG___
    Serial.println("------------------------Iniciando Blynk ------------------------");
  #endif
  Blynk.config(BLYNK_AUTH_TOKEN);  
  while (Blynk.connect() == false) {
    }
  delay (2000);
  init_app_Blynk();

  if (WiFi.status() == WL_CONNECTED){ 
    #ifdef ip_fija
      Mensajes = "Clock Messenger conectado a tu red WIFI IP -> " + ip.toString();
    #else
      Mensajes = "Clock Messenger conectado a tu red WIFI IP -> " + (WiFi.localIP()).toString();
    #endif
    message = true;
  }
  else{
    Mensajes= "Reinicie reloj";
    message = true;
  }

  // Inicializo OTA
  ArduinoOTA.setHostname(hostname); // Hostname OTA
  ArduinoOTA.begin();
  
}

void loop() {

  if (WiFi.status() == WL_CONNECTED){
    ArduinoOTA.handle();    // Actualización código por OTA
    Blynk.run();            // Actualización app Blynk -> Si viene de una desconexión wifi también conecta a Blynk
    conect_wifi = true;
  }

  EasyBuzzer.update();  // Actualización Buzzer

  if (sincronizado){
    // Actualizar y sincronizar hora y fecha del servidor NTP con WIFI
    if ((timeStatus() != timeNotSet) && (WiFi.status() == WL_CONNECTED)){
      if (now() != prevDisplay) { //actualiza el display solo si el tiempo ha cambiado
        prevDisplay = now();
        currentHour = hour(prevDisplay);            // retorna horas -> time t
        currentMinute = minute(prevDisplay);        // retorna minutos -> time t
        currentSecond = second(prevDisplay);        // retorna segundos -> time t
        #ifdef ___RTC___
          //Ajusto RTC con los valores del servidor NTP
          rtc.adjust(DateTime(year(), month(), 
                                  day(), hour(), 
                                  minute(), second())
                            );
          // Verificar RTC
          // WiFi.mode(WIFI_OFF);
          // Serial.println("------------ Desconectado red WIFI ------------ ");
        #endif
      }
      conect_wifi = true;
    }

    #ifdef ___RTC___
      // Actualizar y sincronizar hora y fecha del RTC sin WIFI
      if (WiFi.status() != WL_CONNECTED){
        ahora = rtc.now();                    // fecha y hora actual
        currentSecond = ahora.second();       // retorna segundos -> time t
        currentMinute = ahora.minute();       // retorna minutos -> time t
        currentHour = ahora.hour();           // retorna horas -> time t
        conect_wifi = false;
      }
    #endif
    
    display.update();
    delay(scrollDelay);

    // Envio de mesajes intantaneos por Blink o configurados internamente (básicos y clock)
    if (message){
      retardo_messege();
      send_message();
    }

    // Mensajes Programados
    if ((control_prog == 1)&&(currentHour == prog_hora) && (currentMinute == prog_min)){
      ticker_mensaje_rep.detach();  // detengo mensajes repetitivos
      Mensajes = Men_pro;
      control_prog = 0;
      retardo_messege();
      send_message();
      ticker_delay_prog_60.once_scheduled(65, Act_prog);
      if (control_rep == 1)
          ticker_mensaje_rep.attach_scheduled(rep_min,envio_mensaje_rep);
    } 

    // Mensajes repetitivos
    if ((control_rep == 1)&&(sincro_rep==1)){
      #ifdef ___DEBUG___
        Serial.println("Activados Mensajes Repetitivos");
      #endif
      sincro_rep = 0;
      ticker_mensaje_rep.attach_scheduled(rep_min,envio_mensaje_rep);
    }
    if ((control_rep == 0)&&(sincro_rep==1)){
      #ifdef ___DEBUG___
        Serial.println("Desactivados Mensajes Repetitivos");
      #endif
      sincro_rep = 0;
      ticker_mensaje_rep.detach();
    }

    // Alarma programada_1
    if ((alarma == 1)&&(currentHour == prog_hora) && (currentMinute == prog_min)){
      ticker_mensaje_rep.detach();
      EasyBuzzer.beep(a_frequency, a_onDuration, a_offDuration, a_beeps, a_pauseDuration, a_cycles);
      if (control_rep == 1)
        ticker_mensaje_rep.attach_scheduled(rep_min,envio_mensaje_rep);
      alarma = 0;
      ticker_delay_60.once_scheduled(65, AlarmaON);
    }
    // Alarma programada_2
    if ((alarma == 1)&&(currentHour == prog_hora2) && (currentMinute == prog_min2)){
      ticker_mensaje_rep.detach();
      EasyBuzzer.beep(a_frequency, a_onDuration, a_offDuration, a_beeps, a_pauseDuration, a_cycles);
      if (control_rep == 1)
        ticker_mensaje_rep.attach_scheduled(rep_min,envio_mensaje_rep);
      alarma = 0;
      ticker_delay_60.once_scheduled(65, AlarmaON);
    }

    // No se programa mensaje repetitivo si está vacio
    if ((control_rep == 1)&&(Men_rep=="")){
      control_rep =0;
      Blynk.virtualWrite(V2,control_rep);
      ticker_mensaje_rep.detach();
      #ifdef ___DEBUG___
        Serial.println("Desactivados Mensajes Repetitivos");
      #endif
    }
      
    // No se programa mensaje programado si está vacio
    if ((control_prog == 1)&&(Men_pro=="")){
      control_prog =0;
      Blynk.virtualWrite(V3,control_prog);
      #ifdef ___DEBUG___
        Serial.println("Desactivados Mensajes Programados");
      #endif
    }

  }
  else{
    // Inicializo datos basicos -> actualizar timeZone
    // hasta que se sincronice
    obtenerDatosBasicos();    
    if (timeZone!=10){
      // Servidor NTP
      ntpUDP.begin(localPort);
      setSyncProvider(getNtpTime);
      setSyncInterval(sincro_Clock);
    }
  }

}
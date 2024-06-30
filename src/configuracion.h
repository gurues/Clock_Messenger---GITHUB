#pragma once

#include <ESP8266WiFi.h>

//Descomenta para usar DEBUG por puerto serie
//#define ___DEBUG___
//Descomenta para usar DEBUG1 por puerto serie (Muestra la hora)
//#define ___DEBUG1___

//Descomentar si en Clock_Messenger tiene RTC hardware
#define ___RTC___

const char *hostname = "Clock_Messenger";

// Comenta la siguiente linea para que la IP sea otorgada por el DHCP de tu router
//#define ip_fija

#ifdef ip_fija
    IPAddress ip(192, 168, 1, 161); // Modificar en platformio.ini si se cambia la IP
    IPAddress gateway(192,168,1,1);
    IPAddress subnet (255,255,255,0);
    IPAddress dns1(192,168,1,1);
#endif

// Datos OpenWeatherMap
String hostOpenWeatherMap = "http://api.openweathermap.org/data/2.5/weather";

String apiKey = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXX";    // Sustituye por tu API Key
String idCiudad = "XXXXXXXXXXXXX";                  // Sustituye por el id de tu ciudad -> EJEMPLO: Cambrils = 3126888 Reus = 3111933
             
        
// Parametros constantes a definir por el usuario
const int intensidad = 1;                 // intensidad billo leds
const int offsetdisplay = 0;              // permite compensar la visualización por un número de columnas
const int sincro_Clock = 1800;            // cada X segundos se sincornizar fecha y hora
const int scrollDelay = 100;              // pausa entre rotaciones del mensaje en ms
const int time_data = 600;                // 600 segundos para actualizar los datos basicos, fecha y openweathermap
const int retardoBasico = 120;            // Tiempo en seg de scrolling datos basicos, fecha y openweathermap
const unsigned int frequency = 1500;      // frecuencia del pitido que reproducirá el zumbador o buzzer
const unsigned int onDuration = 500;      // duración del sonido del beep en milisegundos
const unsigned int offDuration = 300;     // duración del silencio del beep en milisegundos
const unsigned int beeps = 2;             // número de pitidos
const unsigned int pauseDuration = 200;   // duración de la pausa entre los diferentes beeps
const unsigned int cycles = 1;            // número de veces que se repiten los números de beeps
const unsigned int a_frequency = 1900;    // frecuencia del pitido que reproducirá el zumbador o buzzer
const unsigned int a_onDuration = 500;    // duración del sonido del beep en milisegundos
const unsigned int a_offDuration = 300;   // duración del silencio del beep en milisegundos
const unsigned int a_beeps = 5;           // número de pitidos
const unsigned int a_pauseDuration = 200; // duración de la pausa entre los diferentes beeps
const unsigned int a_cycles = 4;          // número de veces que se repiten los números de beeps
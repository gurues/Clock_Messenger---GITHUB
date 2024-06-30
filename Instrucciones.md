# CLOCK MESSENGER

## Descripción y funcionamiento

Clock Messenger es un pequeño proyecto que mediante un Wemos D1 mini, una matriz led MAX-7219 FC-16 y un buzzer se crea un dispositivo que mostrará la hora y mensajes básicos repetitivos de fecha y tiempo de la ciudad a elección. También se crea una app en Blynk 2.0 para controlar el dispositivo así como para enviar mensajes instantaneos, repetitivos o programados además de alarma sonora.

![Clock Messenger](/Instrucciones/Fotos/clock_messeger_01.jpg)

Debido a las restricciones de Blynk 2.0 (solo 5 DataStream en su versión libre) utilizaremos un terminal y una serie de comandos para configurar y controlar el Clock Messenger

Por lo tanto debes tener cuenta en Blynk y OpenWeatherMap para actualizar los siguientes parámetros:

main.cpp

```Arduino

// Actualizar con tus datos de BLYNK
#define BLYNK_TEMPLATE_ID "XXXXXXXXXXXXXXXXXX"
#define BLYNK_TEMPLATE_NAME "ClockMessenger"
#define BLYNK_AUTH_TOKEN "XXXXXXXXXXXXXXXXXXX"
```

configuracion.h

```Arduino
// Datos OpenWeatherMap
String hostOpenWeatherMap = "http://api.openweathermap.org/data/2.5/weather";

String apiKey = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXX"; // Sustituye por tu API Key
String idCiudad = "XXXXXXXXXXXXX"; // Sustituye por el id de tu ciudad -> EJEMPLO: Cambrils = 3126888 Reus = 3111933.
```

Los mensajes tratados son:

* Mensajes de reloj. Muestran la hora en pantalla.
* Mensajes básicos. Muestras el tiempo de la ciudad configurada, día y fecha actual.
* Mensajes instantaneos. Enviados por la app de control.
* Mensajes repetitivos. Enviados y configurados por la app de control.
* Mensajes programados. Enviados y configurados por la app de control.

Para la configuración de los mensajes, estos deben comenzar de una forma determinada:

* Mensajes instantaneos: Deben comenzar por el caracter @. Ej: @Hola Mundo. Esto mostrará en la pantalla del reloj el mensaje Hola Mundo.
* Mensajes Repetitivos: Deben comenzar por el caracter R@. Ej: R@Hola Mundo. Esto mostrará en la pantalla del reloj el mensaje Hola Mundo de forma repetitiva
* Mensajes Programados: Deben comenzar por el caracter P@. Ej: P@Hola Mundo. Esto mostrará en la pantalla del reloj el mensaje Hola Mundo a la hora programada.

Disponemos de una serie de comandos que nos ayudaran configurar el dispositivo:

* Comando /MENSAJES/: Muestra los Mensajes Repetitivos y Programados configurados.
* Comando /HELP/: Muestra la Ayuda de comandos.
* Comando /parametro=: El numero después del = se configura como valor del parametro (ej: /offset=-3.5)
* Comando /parametro/: Muestra el valor actual del parametro de control (ej: /offset/)
* Comando /rep_min=: Configura los mensajes se repiten cada x minutos
* Comando /prog_h:min=: Configura hora del mensaje programado o de la alarma 1
* Comando /prog_h:min_2=: Configura hora de la alarma 2
* Comando /sonido=: Activa/Desactiva (1/0) mensajes con alerta sonora
* Comando /alarma=: Activada/Desactivada (1/0) Alarma sonora en Clock_Messenger
* Comando /offset=: Configura el offset de temperatura (ej: /OFFSET=-3.5)
* Comando /ESTADO/: Muestra el estado de los parametros de control Clock_Messenger
* Comando /CLEAR/: Limpia Terminal y vacia mensajes Repetitivos y Programados
  
Tanto los mensajes como los comandos se introduciran a través de la terninal de la aplicación de control del reloj (Blynk 2.0).

DataStream App control Clock Messenger (Blynk 2.0):

* V0. Control de tiempo del mensaje rodante en la pantalla (0 a 300 seg)
* V1. Terminal de control para la configuracón del Clock Messenger mediante comandos y mensajes.
* V2. Control de los mensajes programados (permisivo para la ejecución de mensajes programados)
* V3. Control de los mensajes repetitivos (permisivo para la ejecución de mensajes repetitivos)
* V4. Muestra en el Terminal todos los comandos de configuración del Clock Messenger

El funcionamiento de los mensajes es el siguiente:

* La priorida de los mensajes se determina por el orden de entrada o configuración. El último en entrar o configurarse es el que se muestra en pantalla y por lo tanto el más prioritario. Por eso los mensajes instantaneos son los más prioritarios. Respecto a la prioridad entre los mensajes repetitivos y programados se respeta la misma condición anterior, el último en entrar o configurarse se muestra en pantalla.

* Los mensajes de reloj y datos básicos son los menos prioritarios, siempre se detienen ante la llegada de los otros tipos de mensajes.

* Los mensajes se suceden a través de una serie de funciones hasta que se muestran en pantalla. El flujo de los mensajes y funciones se detalla a continuación:

```text
      retardo_messege() => Configura el tiempo total del mensaje en pantalla.

      send_message() => Envia y muestra el mensaje por pantalla.

      programador() => Controla retorno tras mostrar mensaje y activa las funciones de Clock 
                       y datos básicos.

      clock_send() => Muestra el reloj en pantalla.

      obtenerDatosBasicos() => Obtiene los datos básicos.

      envio_mensaje_rep() => Control de mensajes repetitivos.
```

* Flujo de mensajes instantaneos. Es el mensaje con mayor prioridad, detiene y muestra este mensaje en
  cuanto llegue frente a cualquier otro.

```text
      message = true ==>retardo_messege() -->send_message() -->programador()--> clock_send()

                                                                        |--> obtenerDatosBasicos()
```
  
* Flujo de mensajes programados. Son mensaje prioritarios al reloj y a los datos básicos. Si coinciden en el
  tiempo con estos se detienen y se mostraran los mensajes repetitivos.

```text
control_prog == 1(se cumple hh:mm) ==>retardo_messege() -->send_message() 
                                    
                                                             -->programador() -->clock_send()

                                                                        |--> obtenerDatosBasicos()
```

* Flujo de mensajes repetitivos. Son mensaje prioritarios al reloj y a los datos básicos. Si coinciden en el
  tiempo con estos se detienen y se mostraran los mensajes repetitivos.

```text
se programa tiempo repetición ==>envio_mensaje_rep() -->send_message() 
      
                                                            -->programador() -->clock_send()

                                                                        |--> obtenerDatosBasicos()
```

* El reloj se trata como si fuera un mensaje. Los mensajes de reloj son los menos prioritario.

```text
      clock_send() ==> Se ejecuta cada 1 seg para mostrar el reloj si no hay mensaje que mostrar

```

* Los mensajes de datos basicos se muestran de forma repetida y son los menos prioritarios junto al reloj.

```text  
obtenerDatosBasicos() ==> Se ejecuta cada 95 seg para mostrar el tiempo de la cuidad,el día y la fecha.

```

## Material necesario

* Wemos D1 Mini o ESP8266 NodeMCU.
* Matriz led MAX-7219 FC-16.
* Buzzer activo.
* Resistencia 220 ohm, transistor NPN y diodo (material opcional -> circuito para el contol del Buzzer).
* RTC DS3231 (opcional, solo si no se dispone de conexión permanete a internet).

## Instrucciones

La carpeta instrucciones hay una serie de guias y archivos que son de utilidad para realizar el proyecto.

* Carpeta 3D: Archivos STL o 3MF para la impresión de la caja del reloj.
* Carpeta Blynk: App de control de Clock Messenger.
* Carpeta Conexiones: Esquemas de conexiones del proyecto.
* Carpeta Fotos: Fotos del resultado final.

### No he encontrado el porque pero en unos 7 dias es necesario realizar un reset del reloj para que vuelva funcionar si utilizamos RTC. En tiendo que es un desbordamiento de algún registro pero no lo he podido solucionar. En la parte superior del reloj hay un agujero para que mediante un clip podamos acceder al reset del dispositivo
  
### Realizado por gurues@2024 (<ogurues@gmail.com>)

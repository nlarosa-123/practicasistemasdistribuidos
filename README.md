# practicasistemasdistribuidos

Descripción del proyecto

Este proyecto es un sistema IoT de monitorización de plazas de aparcamiento basado en dos microcontroladores ESP32, desarrollado con FreeRTOS y ESP-IDF. El sistema mide el estado de ocupación de dos plazas mediante sensores ultrasónicos HC-SR04, además de monitorizar temperatura y humedad con un sensor Si7021.

La arquitectura está dividida en dos dispositivos:

* Un ESP32 principal encargado de:

  * Leer sensores de distancia, temperatura y humedad.
  * Determinar si las plazas están libres u ocupadas.
  * Gestionar alarmas de temperatura mediante un LED.
  * Enviar datos en formato JSON por UART y MQTT.
  * Conectarse a Wi-Fi y a un broker MQTT.
  * Implementar actualizaciones OTA usando Mender.
  * Ejecutar tareas concurrentes con FreeRTOS y temporizadores periódicos.

* Un ESP32-C3 secundario encargado de:

  * Recibir los datos enviados por UART.
  * Interpretar el JSON recibido.
  * Encender un LED RGB cuando ambas plazas están ocupadas.

## Funcionamiento general

El sistema realiza mediciones periódicas cada 2 segundos utilizando un temporizador (`esp_timer`). Los datos obtenidos se almacenan en una cola de FreeRTOS y posteriormente son procesados en una tarea dedicada.

### Sensores utilizados

#### Sensores ultrasónicos HC-SR04

Se utilizan dos sensores para medir la distancia hasta un vehículo:

* Si la distancia es menor que un umbral configurado (`CONFIG_DISTANCIA_OCUPADO`), la plaza se considera ocupada.
* Si la distancia supera el umbral, la plaza se considera libre.

También se implementa control de errores:

* Si una lectura falla o supera 160 cm, se reutiliza la última medida válida.

#### Sensor Si7021

Permite medir:

* Temperatura.
* Humedad relativa.

Si la temperatura supera un límite configurado (`CONFIG_TEMP_ALERTA`), se activa un LED de alarma conectado al GPIO2.

---

# Comunicación y conectividad

## Wi-Fi

El ESP32 principal funciona en modo estación y se conecta a una red Wi-Fi configurada desde `menuconfig`.

## MQTT

El sistema publica información en diferentes tópicos MQTT:

* `sed/G02/status`
* `sed/G02/parking/data`
* `sed/G02/actuador/led`

Los datos se envían en formato JSON, por ejemplo:

```json
{
  "temp":24.5,
  "humidity":40.2,
  "p1":1,
  "p2":0,
  "dist1":12.3,
  "dist2":55.1,
  "error1":0,
  "error2":0
}
```

Además:

* Publica el estado “Online/Offline”.
* Permite controlar remotamente el LED mediante mensajes MQTT `"ON"` y `"OFF"`.

---

# Comunicación UART entre ESP32 y ESP32-C3

El ESP32 principal transmite el JSON por UART a un ESP32-C3.

El ESP32-C3:

* Lee continuamente los datos recibidos.
* Analiza el JSON recibido usando `strstr()`.
* Si detecta:

```json
"p1":1
"p2":1
```

enciende un LED RGB rojo indicando que ambas plazas están ocupadas.

---

# OTA y Mender

El proyecto incorpora actualizaciones OTA (Over-The-Air) usando Mender.

Características:

* Identificación única mediante la MAC del dispositivo.
* Verificación automática tras actualización.
* Rollback automático si falla el diagnóstico.
* Gestión del despliegue y reinicio remoto.

El firmware:

* Comprueba si la actualización funciona correctamente.
* Si el Wi-Fi responde adecuadamente, marca la versión como válida.
* Si falla, revierte automáticamente a la versión anterior.

---

# Uso de FreeRTOS

El sistema utiliza:

* Tareas (`xTaskCreate`)
* Colas (`QueueHandle_t`)
* Temporizadores (`esp_timer`)
* Procesamiento concurrente

Esto permite:

* Lectura periódica de sensores.
* Procesamiento desacoplado.
* Comunicación eficiente entre módulos.

## Elementos visuales implementados
## Gauges (indicadores)

Se utilizan indicadores tipo gauge para mostrar:

La temperatura actual medida por el sensor Si7021.
La distancia detectada por cada sensor ultrasónico HC-SR04.

Esto permite conocer rápidamente el estado de cada plaza y las condiciones ambientales.

## Gráficas en tiempo real

El sistema incorpora gráficas dinámicas que muestran la evolución temporal de:

Temperatura.
Humedad.
Distancias medidas por los sensores.

Gracias a estas gráficas es posible observar cambios y tendencias en tiempo real.

## Mensajes y notificaciones

La interfaz también genera mensajes automáticos según el estado del sistema:

Si la temperatura supera el umbral configurado, se muestra un aviso indicando que la temperatura es demasiado alta.
Si una plaza está libre, el sistema lo indica visualmente.
Si una plaza está ocupada, también se notifica.
Si ocurre un error en la lectura de un sensor ultrasónico, se muestra un mensaje de error indicando el fallo de lectura.

---

# Objetivo del proyecto

El objetivo principal es desarrollar una solución IoT completa para aparcamientos inteligentes, integrando:

* Sensórica.
* Comunicaciones inalámbricas.
* MQTT.
* UART.
* Gestión OTA.
* Monitorización remota.
* Procesamiento en tiempo real.
* Arquitectura distribuida entre microcontroladores.

# Instrucciones de compilación e instalación

El proyecto está dividido en dos carpetas:

parking_mqtt      -> Firmware principal ESP32
esp32c3_uart      -> Firmware receptor ESP32-C3

El desarrollo se realiza con ESP-IDF y FreeRTOS.

## 1. Preparar entorno ESP-IDF

Antes de compilar cualquier firmware, cargar el entorno:

source ~/sed/esp-idf/export.sh
## 2. Compilación del firmware principal (parking_mqtt)
Entrar en la carpeta
cd parking_mqtt

Configurar el proyecto
idf.py menuconfig

Configurar:

Wi-Fi SSID.
Password.
Broker MQTT.
Parámetros OTA Mender.
Distancia de ocupación.
Temperatura de alarma.

Archivo relacionado:

nano Kconfig.projbuild
Limpiar compilación anterior
idf.py fullclean

Compilar
idf.py build
Flashear el ESP32

Puerto USB típico:

idf.py -p /dev/ttyUSB0 flash monitor


## 3. Compilación del firmware ESP32-C3
Entrar en la carpeta
cd esp32c3_uart

Seleccionar target ESP32-C3
idf.py set-target esp32c3

Limpiar proyecto
idf.py fullclean

Compilar
idf.py build
Flashear ESP32-C3

idf.py -p /dev/ttyACM0 flash monitor

## 4. Monitorización MQTT
Ver mensajes MQTT
mosquitto_sub -h 10.254.127.217 -t "sed/G02/#" -v

Esto permite visualizar:

Estado online/offline.
Datos del parking.
Comandos LED.

## 5. Gestión del broker Mosquitto
Ver estado
sudo systemctl status mosquitto

Reiniciar broker
sudo systemctl restart mosquitto

## 6. Gestión de Node-RED

Iniciar contenedor
docker start mynodered

Detener contenedor
docker stop mynodered

## 7. Generación del firmware OTA con Mender

Desde la carpeta parking_mqtt:

 /dev/ttyUSB0 flash monitor

 Firmware ESP32-C3
source ~/sed/esp-idf/export.sh

cd esp32c3_uart

idf.py set-target esp32c3

idf.py fullclean

idf.py build

idf.py -p /dev/ttyUSB0 flash monitor

# enlace público al vídeo demostrativo

https://www.youtube.com/watch?v=3L9TYAThWwk




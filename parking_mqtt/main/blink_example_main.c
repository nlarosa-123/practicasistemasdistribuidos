#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>
// Incluimos los drivers del componente externo
#include <si7021.h>
#include <i2cdev.h>
#include "esp_console.h"
#include "freertos/queue.h"

//C3

#include "driver/uart.h"
#include <string.h>

//C3

#define UART_PORT UART_NUM_2
#define TXD_PIN GPIO_NUM_25
#define RXD_PIN GPIO_NUM_26
#define BUF_SIZE 1024

//practica4

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <mqtt_client.h>

//actividad7
#include <esp_system.h>  
#include "esp_ota_ops.h"
#include <inttypes.h>

#define VERSION "1.0.1"

// Includes para Mender
#include <esp_mac.h>
#include "mender-client.h"
#include "mender-flash.h"

//practica 4
#include <stdbool.h>

static const char *TAG = "parking";

// Configuración Mender
// --- Variables globales
mender_client_config_t mender_config;
mender_client_callbacks_t mender_callbacks;
mender_keystore_t mender_identity[2];
char mender_mac_address[18];

//practica3
static esp_mqtt_client_handle_t mqtt_client = NULL;

//practica4

// Sustituye GXX por tu grupo, p.e. G01:
#define LED_GPIO 2
#define TOPIC_STATUS "sed/G02/status"
#define TOPIC_LED    "sed/G02/actuador/led"
#define TOPIC_TEMP   "sed/G02/sensor/temp"
#define TOPIC_D1     "sed/G02/distancia/d1"
#define TOPIC_D2     "sed/G02/distancia/d2"
#define TOPIC_DATA   "sed/G02/parking/data"

// --- Callbacks
static mender_err_t mender_network_connect_cb(void) { return MENDER_OK; }
static mender_err_t mender_network_release_cb(void) { return MENDER_OK; }
static mender_err_t mender_auth_failure_cb(void) { return MENDER_OK; }

static i2c_dev_t dev = { 0 }; // Variable global para acceder desde el callback

typedef struct {
  float temperature;
  float humidity;
  float distance1;
  float distance2;
} sensor_data_t;

static QueueHandle_t sensor_queue;

// Sensor 1
#define TRIG1 GPIO_NUM_18
#define ECHO1 GPIO_NUM_19

// Sensor 2
#define TRIG2 GPIO_NUM_16
#define ECHO2 GPIO_NUM_17

//Practica 4
// Función de diagnóstico: Verifica si estamos conectados al Wi-Fi
bool perform_health_check() {
wifi_ap_record_t ap_info;
// Si podemos leer la info del AP, es que estamos conectados
if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
ESP_LOGI("HEALTH", "Conectado al AP con RSSI: %d", ap_info.rssi);
return true;
}
ESP_LOGE("HEALTH", "Fallo de conexión Wi-Fi.");
return false;
}

// Lógica de supervivencia
void check_and_commit_ota() {
esp_ota_img_states_t ota_state;
const esp_partition_t *running = esp_ota_get_running_partition();
if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
ESP_LOGI("OTA", "Imagen en periodo de prueba. Iniciando auto-diagnóstico...");
if (perform_health_check()) {
ESP_LOGI("OTA", "Salud verificada. Marcando firmware como VLIDO.");
esp_ota_mark_app_valid_cancel_rollback();
} else {
ESP_LOGE("OTA", "Error en diagnóstico. Forzando Rollback y reiniciando...");
esp_ota_mark_app_invalid_rollback_and_reboot();
}
}
}
}

//ACTIVIDAD 7.2
static mender_err_t mender_deployment_status_cb(mender_deployment_status_t status, char *desc) { 
  ESP_LOGI("MENDER", "Estado del despliegue: %s", desc ? desc : "Desconocido");
  return MENDER_OK;
}
static mender_err_t mender_auth_success_cb(void) { 
  ESP_LOGI("MENDER", "Autenticacion exitosa con el servidor");
  // Esto es VITAL: le dice al ESP32 que el firmware funciona y no debe volver a la versión anterior
  return MENDER_OK; 
}

static mender_err_t mender_restart_cb(void) { 
  ESP_LOGI("MENDER", "Reiniciando el sistema por peticion de OTA...");
  esp_restart(); 
  return MENDER_OK; 
}

static float medir_distancia(gpio_num_t trig, gpio_num_t echo)
{
    gpio_set_level(trig, 0);
    esp_rom_delay_us(2);

    gpio_set_level(trig, 1);
    esp_rom_delay_us(10);

    gpio_set_level(trig, 0);

    int64_t start = esp_timer_get_time();

    while (gpio_get_level(echo) == 0)
    {
        if (esp_timer_get_time() - start > 30000)
            return -1;
    }

    int64_t echo_start = esp_timer_get_time();

    while (gpio_get_level(echo) == 1)
    {
        if (esp_timer_get_time() - echo_start > 30000)
            return -1;
    }

    int64_t echo_end = esp_timer_get_time();

    float duration = echo_end - echo_start;

    float distance = duration * 0.0343 / 2.0;

    return distance;
}

// Función Callback del Timer
static void timer_callback(void* arg)
{
    sensor_data_t data;

    // Leer distancias
    data.distance1 = medir_distancia(TRIG1, ECHO1);

    esp_rom_delay_us(50000);

    data.distance2 = medir_distancia(TRIG2, ECHO2);

    // Leer temperatura/humedad
    esp_err_t res1 =
        si7021_measure_temperature(
            &dev,
            &data.temperature);

    esp_err_t res2 =
        si7021_measure_humidity(
            &dev,
            &data.humidity);

    // Si todo OK -> enviar a cola
    if (res1 == ESP_OK && res2 == ESP_OK)
    {
        xQueueSend(sensor_queue, &data, 0);
    }
}

void sensor_processing_task(void *pvParameters)
{
    sensor_data_t received_data;
    
    //PRACTICA3MOSQUITTO
    // Recuperamos el cliente MQTT pasado desde app_main
    
    static float ultima_dist1 = 0;
    static float ultima_dist2 = 0;
    
    int error1 = 0;
    int error2 = 0;

    while (1)
    {
        if (xQueueReceive(
                sensor_queue,
                &received_data,
                portMAX_DELAY))
        {
            ESP_LOGI(TAG, "------------------");

            // SENSOR 1
            if (received_data.distance1 == -1)
            {
                ESP_LOGW(TAG, "Error sensor 1");
            }
            else
            {
                ESP_LOGI(TAG,
                         "Distancia 1: %.2f cm",
                         received_data.distance1);

                if (received_data.distance1 <
                    CONFIG_DISTANCIA_OCUPADO)
                {
                    ESP_LOGI(TAG,
                             "PLAZA 1 OCUPADA");
                }
                else
                {
                    ESP_LOGI(TAG,
                             "PLAZA 1 LIBRE");
                }
            }

            // SENSOR 2
            if (received_data.distance2 == -1)
            {
                ESP_LOGW(TAG, "Error sensor 2");
            }
            else
            {
                ESP_LOGI(TAG,
                         "Distancia 2: %.2f cm",
                         received_data.distance2);

                if (received_data.distance2 <
                    CONFIG_DISTANCIA_OCUPADO)
                {
                    ESP_LOGI(TAG,
                             "PLAZA 2 OCUPADA");
                }
                else
                {
                    ESP_LOGI(TAG,
                             "PLAZA 2 LIBRE");
                }
            }

            // TEMPERATURA
            ESP_LOGI(TAG,
                     "Temperatura: %.2f C",
                     received_data.temperature);

            ESP_LOGI(TAG,
                     "Humedad: %.2f %%",
                     received_data.humidity);

            // LED alarma temperatura
            if (received_data.temperature >
                CONFIG_TEMP_ALERTA)
            {
                ESP_LOGW(TAG,
                         "TEMPERATURA ALTA");

                gpio_set_level(GPIO_NUM_2, 1);
            }
            else
            {
                gpio_set_level(GPIO_NUM_2, 0);
            }
            
            error1 = 0;
            error2 = 0;
              
            //C3
            // SENSOR 1
            if (received_data.distance1 == -1 ||
                received_data.distance1 > 160)
            {
                received_data.distance1 = ultima_dist1;
                error1 = 1;
            }
            else
            {
                ultima_dist1 = received_data.distance1;
            }

            // SENSOR 2
            if (received_data.distance2 == -1 ||
                received_data.distance2 > 160)
            {
                received_data.distance2 = ultima_dist2;
                error2 = 1;
            }
            else
            {
                ultima_dist2 = received_data.distance2;
            }

            char mqtt_msg[128];

            snprintf(
                  mqtt_msg,
                  sizeof(mqtt_msg),
                  "{\"temp\":%.2f,\"humidity\":%.2f,\"p1\":%d,\"p2\":%d,\"dist1\":%.2f,\"dist2\":%.2f,\"error1\":%d,\"error2\":%d}",
                  received_data.temperature,
                  received_data.humidity,
                  received_data.distance1 < CONFIG_DISTANCIA_OCUPADO,
                  received_data.distance2 < CONFIG_DISTANCIA_OCUPADO,
                  received_data.distance1,
                  received_data.distance2,
                  error1,
                  error2
              );

            uart_write_bytes(
                UART_PORT,
                mqtt_msg,
                strlen(mqtt_msg)
            );
            
            //PRACTICA3MOSQUITTO
            esp_mqtt_client_publish(mqtt_client,
              TOPIC_DATA,
              mqtt_msg,
              0,
              1,
              0);
        }
        
    }
}

//C3
void init_uart()
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_driver_install(UART_PORT, BUF_SIZE, 0, 0, NULL, 0);

    uart_param_config(UART_PORT, &uart_config);

    uart_set_pin(
        UART_PORT,
        TXD_PIN,
        RXD_PIN,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    );
}

void wifi_init_sta(void) {
  // Inicializar la pila de red y el bucle de eventos
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  // Configuración por defecto del WiFi
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // Configurar SSID y Password desde el menuconfig
  wifi_config_t wifi_config = {
    .sta = {
      .ssid = CONFIG_ESP_WIFI_SSID,
      //.ssid = "WiFi_No_Existe",
      .password = CONFIG_ESP_WIFI_PASSWORD,
    },
  };

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
  // Arrancar el WiFi
  ESP_ERROR_CHECK(esp_wifi_start());
    
  ESP_LOGI("WIFI", "Conectando a %s...", CONFIG_ESP_WIFI_SSID);
  esp_wifi_connect();
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = event_data;
  esp_mqtt_client_handle_t client = event->client;

  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "Conectado al Broker");
    // Publicar mensaje "Online" para coherencia con LWT
    esp_mqtt_client_publish(
        client,
        TOPIC_STATUS,
        "Online",
        0,
        1,
        1
    );
    // Suscribirse al tópico del LED 
    esp_mqtt_client_subscribe(
        client,
        TOPIC_LED,
        1
    );
    break;

  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "Mensaje recibido en Tópico: %.*s", event->topic_len, event->topic);
    ESP_LOGI(TAG, "Datos: %.*s", event->data_len, event->data);

    // Procesar comando para el LED
    if (strncmp(event->data, "ON", event->data_len) == 0) {
      gpio_set_level(GPIO_NUM_2, 1);
      ESP_LOGI(TAG, "LED encendido");
    } else if (strncmp(event->data, "OFF", event->data_len) == 0) {
      gpio_set_level(GPIO_NUM_2, 0);
      ESP_LOGI(TAG, "LED apagado");
    }
    break;

  case MQTT_EVENT_ERROR:
    ESP_LOGE(TAG, "Error en el stack MQTT");
    break;

  default:
    break;
  }
}

//PRACTICA 3
static esp_mqtt_client_handle_t mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,

        .session.last_will = {
            .topic = TOPIC_STATUS,
            .msg = "Offline",
            .msg_len = strlen("Offline"),
            .qos = 1,
            .retain = 1,
        },

        .session.keepalive = 10,
    };

    esp_mqtt_client_handle_t client =
        esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(
        client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL
    );

    esp_mqtt_client_start(client);

    return client;
}

void app_main(void)
{
    //actividad 7
    const esp_partition_t *running = esp_ota_get_running_partition();
  printf("--- SISTEMA INICIADO ---\n");
  printf("Versión de Firmware: %s\n", VERSION);
  printf("Ejecutando desde partición: %s\n", running->label);
  printf("Dirección Offset: 0x%08" PRIx32 "\n", running->address);
  //Practica3
  // Iniciar WiFi
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  wifi_init_sta(); // Conectamos a la red SED
  
  vTaskDelay(pdMS_TO_TICKS(10000));
  
  // Lógica de Supervivencia (Fase 4)
check_and_commit_ota();

ESP_LOGI("MENDER", "Iniciando cliente...");
  
  
  // --- INICIO CONFIGURACIÓN MENDER ---
  // 1. Obtener la dirección MAC (Mender la exige como identificador único)
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  sprintf(mender_mac_address, "%02x:%02x:%02x:%02x:%02x:%02x", 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  // 2. Identidad
  mender_identity[0].name = "mac";
  mender_identity[0].value = mender_mac_address;
  mender_identity[1].name = NULL;
  mender_identity[1].value = NULL;

  // 3. Configurar los parámetros
  mender_config.identity = mender_identity;
  mender_config.artifact_name = VERSION;
  mender_config.device_type = "esp32";
  mender_config.host = CONFIG_MENDER_SERVER_HOST;
  mender_config.tenant_token = CONFIG_MENDER_SERVER_TENANT_TOKEN;
  mender_config.authentication_poll_interval = 60;
  mender_config.update_poll_interval = 60;
  mender_config.recommissioning = false;
    
  // 4. Definir los callbacks obligatorios
  mender_callbacks.network_connect = mender_network_connect_cb;
  mender_callbacks.network_release = mender_network_release_cb;
  mender_callbacks.authentication_success = mender_auth_success_cb;
  mender_callbacks.authentication_failure = mender_auth_failure_cb;
  mender_callbacks.deployment_status = mender_deployment_status_cb;
  mender_callbacks.restart = mender_restart_cb;
    
  // 5. Arrancar el cliente Mender en segundo plano
  ESP_LOGI("MENDER", "Iniciando cliente con MAC: %s", mender_mac_address);
  if (mender_client_init(&mender_config, &mender_callbacks) == MENDER_OK) {
    mender_client_activate(); // ¡Esto pone a Mender a funcionar en segundo plano!
  } else {
    ESP_LOGE("MENDER", "Fallo al inicializar Mender");
  }
  // --- FIN CONFIGURACIÓN MENDER ---
  
  
  
  // Iniciar MQTT
  // Esperar un poco antes de iniciar MQTT o usar Event Groups
  // Una solución sencilla para el lab es un pequeño retardo,
  // aunque lo ideal es registrar un handler para IP_EVENT_STA_GOT_IP
  vTaskDelay(pdMS_TO_TICKS(10000));
  mqtt_client = mqtt_app_start();
        
  //Practica2
  // HC-SR04
    gpio_reset_pin(TRIG1);
    gpio_set_direction(TRIG1, GPIO_MODE_OUTPUT);

    gpio_reset_pin(ECHO1);
    gpio_set_direction(ECHO1, GPIO_MODE_INPUT);

    gpio_reset_pin(TRIG2);
    gpio_set_direction(TRIG2, GPIO_MODE_OUTPUT);

    gpio_reset_pin(ECHO2);
    gpio_set_direction(ECHO2, GPIO_MODE_INPUT);

    // Inicializar I2C
    ESP_ERROR_CHECK(i2cdev_init());

    // Inicializar Si7021
    ESP_ERROR_CHECK(si7021_init_desc(&dev, 0, 21, 22));
    
    
    
    // Configuramos el GPIO2 para visualizar alarma en LED:
    gpio_reset_pin(GPIO_NUM_2); // Ejercicio: ¿Dónde está definido GPIO_NUM_2?
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT); // ¿Y GPIO_MODE_OUTPUT?
  
    // Crear cola para 10 elementos de tipo sensor_data_t
    sensor_queue = xQueueCreate(10, sizeof(sensor_data_t));

    if (sensor_queue != NULL) {
      xTaskCreate(sensor_processing_task, "proc_task", 4096, NULL, 10, NULL);
    }
    
    
    // Inicializar UART
    init_uart();


    // Configuración timer
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &timer_callback,
        .name = "parking_timer"
    };

    esp_timer_handle_t periodic_timer;

    ESP_ERROR_CHECK(
        esp_timer_create(
            &periodic_timer_args,
            &periodic_timer));

    ESP_LOGI(TAG, "Iniciando timer periódico...");

    // 2 segundos
    ESP_ERROR_CHECK(
        esp_timer_start_periodic(
            periodic_timer,
            2000000));
}

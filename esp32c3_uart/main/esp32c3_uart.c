#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "driver/gpio.h"

#include "esp_log.h"

#include "led_strip.h"

static const char *TAG = "C3";

#define UART_PORT UART_NUM_1

#define TXD_PIN GPIO_NUM_21
#define RXD_PIN GPIO_NUM_20

#define BUF_SIZE 1024

#define LED_GPIO GPIO_NUM_2

static led_strip_handle_t led_strip;

void init_uart()
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
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

void init_led()
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };

    ESP_ERROR_CHECK(
        led_strip_new_rmt_device(
            &strip_config,
            &rmt_config,
            &led_strip
        )
    );

    led_strip_clear(led_strip);
}

void app_main()
{
    init_uart();

    init_led();

    uint8_t data[BUF_SIZE];

    while (1)
    {
        int len = uart_read_bytes(
            UART_PORT,
            data,
            BUF_SIZE - 1,
            pdMS_TO_TICKS(1000)
        );

        if (len > 0)
        {
            data[len] = 0;

            ESP_LOGI(TAG, "Recibido: %s", data);

            if (strstr((char*)data, "\"p1\":1") &&
                strstr((char*)data, "\"p2\":1"))
            {
                led_strip_set_pixel(led_strip, 0, 255, 0, 0);
                led_strip_refresh(led_strip);
            }
            else
            {
                led_strip_clear(led_strip);
            }
        }
    }
}

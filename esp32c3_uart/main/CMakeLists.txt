idf_component_register(SRCS "esp32c3_uart.c"
                    INCLUDE_DIRS "."
		    REQUIRES led_strip esp_driver_uart esp_driver_gpio)

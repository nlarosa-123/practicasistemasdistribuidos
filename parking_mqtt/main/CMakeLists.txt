idf_component_register(SRCS "blink_example_main.c"
                       INCLUDE_DIRS "."
			REQUIRES 
			console 
			esp_timer 
			esp_driver_uart 
			esp_driver_gpio 
			mqtt
			esp_wifi
			nvs_flash
			esp_event
			esp_netif
			app_update
			mqtt
			driver
			esp_partition
			esp-idf
)

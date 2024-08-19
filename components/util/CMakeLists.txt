idf_component_register(SRCS          util_fan.c util_generic.c util_gpio.c util_httpd.c util_buffer.c util_nvs.c util_disp_selection.c util_brightness.c util_fixed_point.c
                       REQUIRES      esp_driver_gpio esp_http_server json nvs_flash
                       PRIV_REQUIRES esp_adc esp_driver_ledc esp-tls 
                       INCLUDE_DIRS  include)
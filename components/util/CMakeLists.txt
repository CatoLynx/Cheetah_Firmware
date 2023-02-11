idf_component_register(SRCS         util_fan.c util_generic.c util_gpio.c util_httpd.c util_buffer.c util_nvs.c
                       REQUIRES     esp_http_server nvs_flash esp-tls
                       INCLUDE_DIRS include)
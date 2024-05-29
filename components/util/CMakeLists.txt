idf_component_register(SRCS         util_fan.c util_generic.c util_gpio.c util_httpd.c util_buffer.c util_nvs.c util_disp_selection.c util_brightness.c
                       REQUIRES     esp_http_server nvs_flash esp-tls json
                       INCLUDE_DIRS include)
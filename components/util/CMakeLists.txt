idf_component_register(SRCS         util_fan.c util_generic.c util_gpio.c util_httpd.c util_buffer.c
                       REQUIRES     esp_http_server
                       INCLUDE_DIRS include)
idf_component_register(SRCS           remote_poll.c
                       INCLUDE_DIRS   include
                       REQUIRES       esp_http_client json nvs_flash util mbedtls)
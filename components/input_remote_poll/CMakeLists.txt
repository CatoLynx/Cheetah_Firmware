idf_component_register(SRCS           remote_poll.c
                       INCLUDE_DIRS   include
                       REQUIRES       esp_http_client json nvs_flash
                       PRIV_REQUIRES  esp_netif esp_timer mbedtls util)
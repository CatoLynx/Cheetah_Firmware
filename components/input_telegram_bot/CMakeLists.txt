idf_component_register(SRCS           telegram_bot.c
                       INCLUDE_DIRS   include
                       REQUIRES       esp_http_client json nvs_flash
                       PRIV_REQUIRES  esp_netif esp_timer util)
idf_component_register(SRCS           browser_config.c
                       INCLUDE_DIRS   include
                       REQUIRES       esp_http_server nvs_flash
                       PRIV_REQUIRES  json util
                       EMBED_TXTFILES static/browser_config.html)
idf_component_register(SRCS           browser_spiffs.c
                       INCLUDE_DIRS   include
                       REQUIRES       esp_http_server json spiffs
                       PRIV_REQUIRES  util
                       EMBED_TXTFILES static/browser_spiffs.html)
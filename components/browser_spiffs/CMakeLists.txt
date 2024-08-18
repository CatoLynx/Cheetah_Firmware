idf_component_register(SRCS           browser_spiffs.c
                       INCLUDE_DIRS   include
                       REQUIRES       esp_http_server
                       PRIV_REQUIRES  json spiffs util
                       EMBED_TXTFILES static/browser_spiffs.html)
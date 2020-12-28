idf_component_register(SRCS           browser_ota.c
                       INCLUDE_DIRS   include
                       REQUIRES       esp_http_server
                       PRIV_REQUIRES  app_update
                       EMBED_TXTFILES static/browser_ota.html static/spinner.gif)
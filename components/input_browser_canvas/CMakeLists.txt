idf_component_register(SRCS           browser_canvas.c
                       INCLUDE_DIRS   include
                       REQUIRES       esp_http_server json mbedtls
                       PRIV_REQUIRES  util
                       EMBED_TXTFILES static/browser_canvas.html)
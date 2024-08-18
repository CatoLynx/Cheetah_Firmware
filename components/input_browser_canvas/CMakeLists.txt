idf_component_register(SRCS           browser_canvas.c
                       INCLUDE_DIRS   include
                       REQUIRES       esp_http_server json nvs_flash
                       PRIV_REQUIRES  effects_char input_bitmap_generators mbedtls shaders_char transitions_pixel  util
                       EMBED_TXTFILES static/browser_canvas.html)
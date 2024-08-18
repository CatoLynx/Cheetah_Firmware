idf_component_register(SRCS           browser_canvas.c
                       INCLUDE_DIRS   include
                       REQUIRES       esp_http_server json mbedtls
                       PRIV_REQUIRES  util shaders_char effects_char transitions_pixel input_bitmap_generators
                       EMBED_TXTFILES static/browser_canvas.html)
idf_component_register(SRCS          char_16seg_led_ws281x_hybrid.c
                       INCLUDE_DIRS  include
                       REQUIRES      nvs_flash
                       PRIV_REQUIRES esp_driver_spi shaders_char transitions_pixel util)
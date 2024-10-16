idf_component_register(SRCS          char_16seg_led_ws281x.c
                       INCLUDE_DIRS  include
                       REQUIRES      nvs_flash
                       PRIV_REQUIRES esp_driver_spi effects_char shaders_char util)
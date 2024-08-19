idf_component_register(SRCS          char_16seg_led_spi.c
                       INCLUDE_DIRS  include
                       REQUIRES      nvs_flash
                       PRIV_REQUIRES esp_driver_gpio esp_driver_ledc esp_driver_spi effects_char util)
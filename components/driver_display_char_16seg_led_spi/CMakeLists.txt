idf_component_register(SRCS          char_16seg_led_spi.c
                       INCLUDE_DIRS  include
                       REQUIRES      nvs_flash
                       PRIV_REQUIRES driver effects_char util)
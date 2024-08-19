idf_component_register(SRCS          char_ibis.c
                       INCLUDE_DIRS  include
                       REQUIRES      nvs_flash
                       PRIV_REQUIRES esp_driver_spi esp_driver_uart shaders_char util)
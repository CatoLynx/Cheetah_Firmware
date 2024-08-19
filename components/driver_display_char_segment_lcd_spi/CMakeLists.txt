idf_component_register(SRCS          char_segment_lcd_spi.c
                       INCLUDE_DIRS  include
                       REQUIRES      nvs_flash
                       PRIV_REQUIRES esp_driver_ledc esp_driver_spi util)
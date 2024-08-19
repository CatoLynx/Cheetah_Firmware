idf_component_register(SRCS          char_segment_lcd_spi.c
                       INCLUDE_DIRS  include
                       REQUIRES      nvs_flash
                       PRIV_REQUIRES driver util)
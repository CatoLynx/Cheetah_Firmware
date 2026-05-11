idf_component_register(SRCS          sel_aeg_splitflap.c
                       INCLUDE_DIRS  include
                       REQUIRES      nvs_flash
                       PRIV_REQUIRES esp_driver_spi esp_timer util)
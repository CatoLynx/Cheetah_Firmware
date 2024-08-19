idf_component_register(SRCS          led_shift_register.c led_shift_register_i2s.c
                       INCLUDE_DIRS  include
                       REQUIRES      nvs_flash
                       PRIV_REQUIRES esp_driver_spi i2s_parallel util)
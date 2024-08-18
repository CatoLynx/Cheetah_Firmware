idf_component_register(SRCS          led_aesys_i2s.c
                       INCLUDE_DIRS  include
                       REQUIRES      nvs_flash
                       PRIV_REQUIRES i2s_parallel util)
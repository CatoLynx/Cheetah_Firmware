idf_component_register(SRCS          char_ibis.c
                       INCLUDE_DIRS  include
                       REQUIRES      nvs_flash
                       PRIV_REQUIRES driver shaders_char util)
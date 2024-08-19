idf_component_register(SRCS          char_k9000.c
                       INCLUDE_DIRS  include
                       REQUIRES      nvs_flash
                       PRIV_REQUIRES driver util)
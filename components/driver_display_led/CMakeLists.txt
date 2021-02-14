idf_component_register(SRCS         led_shift_register.c led_shift_register_i2s.c
                       INCLUDE_DIRS include
                       REQUIRES     i2s_parallel util)
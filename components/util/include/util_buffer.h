#pragma once

#include <stdint.h>

typedef enum {
    MT_OVERWRITE,
    MT_KEEP_1,
    MT_KEEP_0
} buf_merge_t;


void buffer_8to1(uint8_t* buf8, uint8_t* buf1, uint16_t width, uint16_t height, buf_merge_t mergeType);
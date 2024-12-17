#pragma once

#include <stdint.h>

#include "cJSON.h"


void bitmap_generators_init(uint8_t* pixBuf, size_t pixBufSize, portMUX_TYPE* pixBufLock, uint16_t frameWidth, uint16_t frameHeight);
void bitmap_generator_select(cJSON* bitmapGeneratorData);
cJSON* bitmap_generators_get_available();
void bitmap_generator_current(int64_t t);
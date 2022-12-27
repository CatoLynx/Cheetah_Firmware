#pragma once

#include <stdint.h>
#include "util_generic.h"

color_rgb_t effect_static_rainbow(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character, void* effectParams);
color_rgb_t effect_sweeping_rainbow(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character, void* effectParams);
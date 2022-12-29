#pragma once

#include <stdint.h>
#include "util_generic.h"
#include "cJSON.h"

cJSON* shader_get_available();
color_rgb_t shader_static(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character, color_rgb_t color);
color_rgb_t shader_static_rainbow(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character);
color_rgb_t shader_sweeping_rainbow(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character, uint16_t speed);
color_rgb_t shader_linear_gradient(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character, color_rgb_t start, color_rgb_t end);
color_rgb_t shader_fromJSON(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character, cJSON* shaderData);
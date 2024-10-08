#pragma once

#include "fixed_point.h"
#include "util_generic.h"

typedef struct {
    fx20_12_t h;
    fx20_12_t s;
    fx20_12_t v;
} color_hsv_fx20_12_t;


fx20_12_t sin_i16_to_fx20_12(int16_t i);
fx20_12_t cos_i16_to_fx20_12(int16_t i);
fx20_12_t sqrt_i32_to_fx20_12(int32_t v);
color_rgb_u8_t hsv_fx20_12_to_rgb_u8(color_hsv_fx20_12_t hsv);
color_hsv_fx20_12_t rgb_u8_to_hsv_fx20_12(color_rgb_u8_t rgb);
int32_t interpolate_fx20_12_i32(fx20_12_t val, int32_t start, int32_t end);
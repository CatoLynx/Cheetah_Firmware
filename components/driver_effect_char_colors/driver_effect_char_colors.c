/*
 * Functions for character display color effects
 */

#include "driver_effect_char_colors.h"


#define LOG_TAG "EFF-CHAR-COLOR"


color_rgb_t effect_static_rainbow(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character, void* effectParams) {
    color_hsv_t calcColor_hsv;
    calcColor_hsv.h = cb_i_display * (360 / charBufSize);
    calcColor_hsv.s = 1.0;
    calcColor_hsv.v = 1.0;
    return hsv2rgb(calcColor_hsv);
}

color_rgb_t effect_sweeping_rainbow(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character, void* effectParams) {
    uint16_t speed = (uint16_t)effectParams;
    color_hsv_t calcColor_hsv;
    calcColor_hsv.h = cb_i_display * (360 / charBufSize);
    calcColor_hsv.h += speed * time_getSystemTime_us() / 1000000;
    calcColor_hsv.h = (uint16_t)calcColor_hsv.h % 360;
    calcColor_hsv.s = 1.0;
    calcColor_hsv.v = 1.0;
    return hsv2rgb(calcColor_hsv);
}
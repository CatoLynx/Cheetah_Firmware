#pragma once

#include "esp_system.h"

#define DIV_CEIL(x, y) ((x % y) ? x / y + 1 : x / y)

#define I8_MIN  ((int8_t)0xFF)
#define I16_MIN ((int16_t)0xFFFF)
#define I32_MIN ((int32_t)0xFFFFFFFF)
#define I64_MIN ((int64_t)0xFFFFFFFFFFFFFFFF)

#define I8_MAX  ((int8_t)0x7F)
#define I16_MAX ((int16_t)0x7FFF)
#define I32_MAX ((int32_t)0x7FFFFFFF)
#define I64_MAX ((int64_t)0x7FFFFFFFFFFFFFFF)

#define U8_MIN  ((uint8_t)0)
#define U16_MIN ((uint16_t)0)
#define U32_MIN ((uint32_t)0)
#define U64_MIN ((uint64_t)0)

#define U8_MAX  ((uint8_t)0xFF)
#define U16_MAX ((uint16_t)0xFFFF)
#define U32_MAX ((uint32_t)0xFFFFFFFF)
#define U64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFF)

typedef struct {
    double r;       // a fraction between 0 and 1
    double g;       // a fraction between 0 and 1
    double b;       // a fraction between 0 and 1
} color_rgb_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color_rgb_u8_t;

typedef struct {
    double h;       // angle in degrees
    double s;       // a fraction between 0 and 1
    double v;       // a fraction between 0 and 1
} color_hsv_t;

uint8_t count_set_bits(uint8_t byte);
uint8_t int_num_digits(int64_t n, uint8_t includeNegSign);
uint8_t uint_num_digits(uint64_t n);
uint8_t uint8_to_bcd(uint8_t n);
void str_toUpper(char* str);
void str_filterAllowed(char* out, char* in, char* allowedChars, bool allowLineBreaks);
void str_filterDisallowed(char* out, char* in, char* disallowedChars, bool allowLineBreaks);
void str_filterRangeAllowed(char* out, char* in, uint8_t rangeMin, uint8_t rangeMax, bool allowLineBreaks);
void str_filterRangDisallowed(char* out, char* in, uint8_t rangeMin, uint8_t rangeMax, bool allowLineBreaks);
void str_convertLineBreaks(char* out, char* in, uint16_t numLines, uint16_t charsPerLine);
void str_insertLineBreaks(char* out, char* in, uint16_t interval, size_t inLen);
color_hsv_t rgb2hsv(color_rgb_t in);
color_rgb_t hsv2rgb(color_hsv_t in);
int64_t time_getSystemTime_us();
int32_t map_int32(int32_t in, int32_t inMin, int32_t inMax, int32_t outMin, int32_t outMax);
double map_double(double in, double inMin, double inMax, double outMin, double outMax);
int32_t rand_range(int32_t min, int32_t max);
int32_t rand_spread(int32_t nominal, int32_t spread);
#include "util_fixed_point.h"

fx20_12_t sin_i16_to_fx20_12(int16_t i)
{
    /* Convert (signed) input to a value between 0 and 8192. (8192 is pi/2, which is the region of the curve fit). */
    /* ------------------------------------------------------------------- */
    i <<= 1;
    uint8_t c = i<0; //set carry for output pos/neg

    if(i == (i|0x4000)) // flip input value to corresponding value in range [0..8192)
        i = (1<<15) - i;
    i = (i & 0x7FFF) >> 1;
    /* ------------------------------------------------------------------- */

    /* The following section implements the formula:
     = y * 2^-n * ( A1 - 2^(q-p)* y * 2^-n * y * 2^-n * [B1 - 2^-r * y * 2^-n * C1 * y]) * 2^(a-q)
    Where the constants are defined as follows:
    */
    enum {A1=3370945099UL, B1=2746362156UL, C1=292421UL};
    enum {n=13, p=32, q=31, r=3, a=12};

    uint32_t y = (C1*((uint32_t)i))>>n;
    y = B1 - (((uint32_t)i*y)>>r);
    y = (uint32_t)i * (y>>n);
    y = (uint32_t)i * (y>>n);
    y = A1 - (y>>(p-q));
    y = (uint32_t)i * (y>>n);
    y = (y+(1UL<<(q-a-1)))>>(q-a); // Rounding

    return c ? -y : y;
}

fx20_12_t cos_i16_to_fx20_12(int16_t i)
{
    return sin_i16_to_fx20_12(i + 0x2000);
}

fx20_12_t sqrt_i32_to_fx20_12(int32_t v) {
    // sqrt with 16 fractional bits (gets converted to 12 bits), input is a regular integer
    uint32_t t, q, b, r;
    if (v == 0) return 0;
    r = v;
    b = 0x40000000;
    q = 0;
    while( b > 0 )
    {
        t = q + b;
        if( r >= t )
        {
            r -= t;
            q = t + b;
        }
        r <<= 1;
        b >>= 1;
    }
    if( r > q ) ++q;
    return (fx20_12_t)q >> 4;
}

color_rgb_u8_t hsv_fx20_12_to_rgb_u8(color_hsv_fx20_12_t hsv) {
    fx20_12_t hue_fp, p_fp, q_fp, t_fp, remainder_fp;
    uint8_t sector;
    color_rgb_u8_t out;

    if(hsv.s == 0) {
        out.r = UNFX20_12_ROUND(hsv.v * 255);
        out.g = UNFX20_12_ROUND(hsv.v * 255);
        out.b = UNFX20_12_ROUND(hsv.v * 255);
        return out;
    }
    hue_fp = hsv.h;
    if(hue_fp < 0 || hue_fp >= FX20_12(360)) hue_fp = 0;
    hue_fp /= 60;
    sector = (uint8_t)UNFX20_12(hue_fp);
    remainder_fp = hue_fp - FX20_12(sector);
    p_fp = UNFX20_12(hsv.v * (FX20_12(1) - hsv.s));
    q_fp = UNFX20_12(hsv.v * (FX20_12(1) - UNFX20_12(hsv.s * remainder_fp)));
    t_fp = UNFX20_12(hsv.v * (FX20_12(1) - UNFX20_12(hsv.s * (FX20_12(1) - remainder_fp))));

    switch(sector) {
    case 0:
        out.r = UNFX20_12_ROUND(hsv.v * 255);
        out.g = UNFX20_12_ROUND(t_fp * 255);
        out.b = UNFX20_12_ROUND(p_fp * 255);
        break;
    case 1:
        out.r = UNFX20_12_ROUND(q_fp * 255);
        out.g = UNFX20_12_ROUND(hsv.v * 255);
        out.b = UNFX20_12_ROUND(p_fp * 255);
        break;
    case 2:
        out.r = UNFX20_12_ROUND(p_fp * 255);
        out.g = UNFX20_12_ROUND(hsv.v * 255);
        out.b = UNFX20_12_ROUND(t_fp * 255);
        break;
    case 3:
        out.r = UNFX20_12_ROUND(p_fp * 255);
        out.g = UNFX20_12_ROUND(q_fp * 255);
        out.b = UNFX20_12_ROUND(hsv.v * 255);
        break;
    case 4:
        out.r = UNFX20_12_ROUND(t_fp * 255);
        out.g = UNFX20_12_ROUND(p_fp * 255);
        out.b = UNFX20_12_ROUND(hsv.v * 255);
        break;
    case 5:
    default:
        out.r = UNFX20_12_ROUND(hsv.v * 255);
        out.g = UNFX20_12_ROUND(p_fp * 255);
        out.b = UNFX20_12_ROUND(q_fp * 255);
        break;
    }
    return out;     
}

color_hsv_fx20_12_t rgb_u8_to_hsv_fx20_12(color_rgb_u8_t rgb) {
    color_hsv_fx20_12_t hsv_fx;
    uint8_t max = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);
    uint8_t min = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
    
    int32_t delta = max - min;

    // Calculate value
    hsv_fx.v = FX20_12(max) / 255;

    // Calculate saturation
    if (max != 0) {
        hsv_fx.s = FX20_12(delta) / max;
    } else {
        hsv_fx.s = 0;
    }

    // Calculate hue
    if (delta == 0) {
        hsv_fx.h = 0;
    } else {
        if (max == rgb.r) {
            hsv_fx.h = 60 * ((rgb.g - rgb.b) * FX20_12(1) / delta % FX20_12(6));
        } else if (max == rgb.g) {
            hsv_fx.h = 60 * ((rgb.b - rgb.r) * FX20_12(1) / delta + FX20_12(2));
        } else {
            hsv_fx.h = 60 * ((rgb.r - rgb.g) * FX20_12(1) / delta + FX20_12(4));
        }
    }

    if (hsv_fx.h < 0) {
        hsv_fx.h += FX20_12(360);
    }

    return hsv_fx;
}

int32_t interpolate_fx20_12_i32(fx20_12_t val, int32_t start, int32_t end) {
    // val: 0 ... 1; 0 = start, 1 = end
    int32_t range = end - start;
    fx20_12_t scaled_range_fx = val * range;
    int32_t result = start + UNFX20_12_ROUND(scaled_range_fx);
    return result;
}
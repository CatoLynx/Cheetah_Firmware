#define _USE_MATH_DEFINES
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>

#include "char_16seg_mapping.h"

#define CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP

typedef int32_t fx20_12_t;
typedef int64_t fx52_12_t;

#define FX20_12(num) (((fx20_12_t)num) << 12)
#define UNFX20_12(num) ((num) >> 12)
#define UNFX20_12_ROUND(num) (UNFX20_12(num + (1 << 11)))

#define FX52_12(num) (((fx52_12_t)num) << 12)
#define UNFX52_12(num) ((num) >> 12)
#define UNFX52_12_ROUND(num) (UNFX52_12(num + (1 << 11)))

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color_rgb_u8_t;

typedef struct {
    fx20_12_t h;
    fx20_12_t s;
    fx20_12_t v;
} color_hsv_fx20_12_t;


uint8_t* pixel_buffer;
uint32_t pixel_buffer_size;
uint32_t frame_width;
uint32_t frame_height;


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

color_rgb_u8_t hsv_fx20_12_to_rgb_u8(color_hsv_fx20_12_t in) {
    fx20_12_t hue_fp, p_fp, q_fp, t_fp, remainder_fp;
    uint8_t sector;
    color_rgb_u8_t out;

    if(in.s == 0) {
        out.r = UNFX20_12_ROUND(in.v * 255);
        out.g = UNFX20_12_ROUND(in.v * 255);
        out.b = UNFX20_12_ROUND(in.v * 255);
        return out;
    }
    hue_fp = in.h;
    if(hue_fp < 0 || hue_fp >= FX20_12(360)) hue_fp = 0;
    hue_fp /= 60;
    sector = (uint8_t)UNFX20_12(hue_fp);
    remainder_fp = hue_fp - FX20_12(sector);
    p_fp = UNFX20_12(in.v * (FX20_12(1) - in.s));
    q_fp = UNFX20_12(in.v * (FX20_12(1) - UNFX20_12(in.s * remainder_fp)));
    t_fp = UNFX20_12(in.v * (FX20_12(1) - UNFX20_12(in.s * (FX20_12(1) - remainder_fp))));

    switch(sector) {
    case 0:
        out.r = UNFX20_12_ROUND(in.v * 255);
        out.g = UNFX20_12_ROUND(t_fp * 255);
        out.b = UNFX20_12_ROUND(p_fp * 255);
        break;
    case 1:
        out.r = UNFX20_12_ROUND(q_fp * 255);
        out.g = UNFX20_12_ROUND(in.v * 255);
        out.b = UNFX20_12_ROUND(p_fp * 255);
        break;
    case 2:
        out.r = UNFX20_12_ROUND(p_fp * 255);
        out.g = UNFX20_12_ROUND(in.v * 255);
        out.b = UNFX20_12_ROUND(t_fp * 255);
        break;
    case 3:
        out.r = UNFX20_12_ROUND(p_fp * 255);
        out.g = UNFX20_12_ROUND(q_fp * 255);
        out.b = UNFX20_12_ROUND(in.v * 255);
        break;
    case 4:
        out.r = UNFX20_12_ROUND(t_fp * 255);
        out.g = UNFX20_12_ROUND(p_fp * 255);
        out.b = UNFX20_12_ROUND(in.v * 255);
        break;
    case 5:
    default:
        out.r = UNFX20_12_ROUND(in.v * 255);
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

    // Calculate Value
    hsv_fx.v = FX20_12(max) / 255;

    // Calculate Saturation
    if (max != 0) {
        hsv_fx.s = FX20_12(delta) / max;
    } else {
        hsv_fx.s = 0;
    }

    // Calculate Hue
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


void bitmap_generator_solid_single(int64_t t, color_rgb_u8_t color) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    for (uint16_t i = 0; i < MAPPING_LENGTH; i++) {
        uint16_t pixBufIndex = LED_TO_BITMAP_MAPPING[i];
        
        pixel_buffer[pixBufIndex] = color.r;
        pixel_buffer[pixBufIndex + 1] = color.g;
        pixel_buffer[pixBufIndex + 2] = color.b;
    }
    #endif
}

void bitmap_generator_rainbow_t(int64_t t, uint16_t speed) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    color_hsv_fx20_12_t calcColor_hsv_fx;
    fx52_12_t t_sec_fx = FX52_12(t) / 1000000; // It's a UNIX timestamp, so it needs more bits
    fx52_12_t temp_fx = speed * t_sec_fx;
    calcColor_hsv_fx.h = temp_fx % FX20_12(360);
    calcColor_hsv_fx.s = FX20_12(1);
    calcColor_hsv_fx.v = FX20_12(1);
    color_rgb_u8_t color = hsv_fx20_12_to_rgb_u8(calcColor_hsv_fx);
    bitmap_generator_solid_single(t, color);
    #endif
}

void bitmap_generator_rainbow_gradient(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, uint8_t s, uint8_t v) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    int32_t angle_units = (angle * 0x8000) / 360;
    fx20_12_t sin_angle_fx = sin_i16_to_fx20_12(angle_units);
    fx20_12_t cos_angle_fx = cos_i16_to_fx20_12(angle_units);
    fx20_12_t normalized_scale_fx = FX20_12(scale) / 100;
    fx20_12_t normalized_s_fx = FX20_12(s) / 255;
    fx20_12_t normalized_v_fx = FX20_12(v) / 255;

    // Calculate the diagonal length of the frame
    fx20_12_t diagonal_length_fx = sqrt_i32_to_fx20_12(frame_width * frame_width + frame_height * frame_height);

    for (uint16_t i = 0; i < MAPPING_LENGTH; i++) {
        uint16_t pixBufIndex = LED_TO_BITMAP_MAPPING[i];
        uint16_t x = pixBufIndex / (frame_height * 3);
        uint16_t y = pixBufIndex % (frame_height * 3) / 3;
        
        // Calculate the distance along the gradient direction
        fx20_12_t distance_along_gradient_fx = x * cos_angle_fx + y * sin_angle_fx;

        // Normalize the distance by the diagonal length to get a value between 0 and 1
        fx20_12_t normalized_distance_fx = FX20_12((int64_t)distance_along_gradient_fx) / diagonal_length_fx;

        // Scale the normalized distance to 360 degrees, add 1 to ensure we are positive
        fx20_12_t offset_fx = (normalized_distance_fx + FX20_12(1)) * 360;

        // Apply manual scaling
        offset_fx = UNFX20_12((int64_t)offset_fx * normalized_scale_fx);
        
        color_hsv_fx20_12_t hsv_fx;
        fx20_12_t normalized_speed_fx = speed * normalized_scale_fx; // Use scale to adjust speed so it appears constant
        fx52_12_t t_sec_fx = FX52_12(t) / 1000000; // It's a UNIX timestamp, so it needs more bits
        fx52_12_t temp_fx = UNFX52_12((int64_t)normalized_speed_fx * t_sec_fx) + offset_fx;
        hsv_fx.h = temp_fx % FX20_12(360);
        hsv_fx.s = normalized_s_fx;
        hsv_fx.v = normalized_v_fx;

        // Get the color for the current segment
        color_rgb_u8_t rgb_u8 = hsv_fx20_12_to_rgb_u8(hsv_fx);
        pixel_buffer[pixBufIndex] = rgb_u8.r;
        pixel_buffer[pixBufIndex + 1] = rgb_u8.g;
        pixel_buffer[pixBufIndex + 2] = rgb_u8.b;
    }
    #endif
}

void bitmap_generator_hard_gradient(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, uint16_t numColors, color_rgb_u8_t* colors) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    int32_t angle_units = (angle * 0x8000) / 360;
    fx20_12_t sin_angle_fx = sin_i16_to_fx20_12(angle_units);
    fx20_12_t cos_angle_fx = cos_i16_to_fx20_12(angle_units);
    fx20_12_t normalized_scale_fx = FX20_12(scale) / 100;

    // Calculate the diagonal length of the frame
    fx20_12_t diagonal_length_fx = sqrt_i32_to_fx20_12(frame_width * frame_width + frame_height * frame_height);
    
    for (uint16_t i = 0; i < MAPPING_LENGTH; i++) {
        uint32_t pixBufIndex = LED_TO_BITMAP_MAPPING[i];
        uint16_t x = pixBufIndex / (frame_height * 3);
        uint16_t y = pixBufIndex % (frame_height * 3) / 3;
        
        // Calculate the distance along the gradient direction
        fx20_12_t distance_along_gradient_fx = x * cos_angle_fx + y * sin_angle_fx;

        // Normalize the distance by the diagonal length to get a value between 0 and 1
        fx20_12_t normalized_distance_fx = FX20_12((int64_t)distance_along_gradient_fx) / diagonal_length_fx;

        // Add 1 to ensure we are positive
        fx20_12_t offset_fx = normalized_distance_fx + FX20_12(1);
        
        // Apply manual scaling
        offset_fx = UNFX20_12((int64_t)offset_fx * normalized_scale_fx);
        
        fx20_12_t normalized_speed_fx = (speed * normalized_scale_fx) / 100;
        fx52_12_t t_sec_fx = FX52_12(t) / 1000000; // It's a UNIX timestamp, so it needs more bits
        fx52_12_t temp_fx = UNFX52_12((int64_t)normalized_speed_fx * t_sec_fx) + offset_fx;
        temp_fx %= FX20_12(1);

        // Determine the segment index
        uint16_t segment_index = (uint16_t)UNFX20_12_ROUND(temp_fx * numColors) % numColors;

        // Get the color for the current segment
        color_rgb_u8_t color = colors[segment_index];
        pixel_buffer[pixBufIndex] = color.r;
        pixel_buffer[pixBufIndex + 1] = color.g;
        pixel_buffer[pixBufIndex + 2] = color.b;
    }
    #endif
}

void bitmap_generator_hard_gradient_3(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, color_rgb_u8_t color1, color_rgb_u8_t color2, color_rgb_u8_t color3) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    color_rgb_u8_t colors[3] = {color1, color2, color3};
    bitmap_generator_hard_gradient(t, speed, angle, scale, 3, colors);
    #endif
}

void bitmap_generator_on_off_100_frames(int64_t t) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    static uint8_t on_off_100_frames_n = 0;
    static uint8_t on_off_100_frames_val = 255;
    
    for (uint16_t i = 0; i < MAPPING_LENGTH; i++) {
        uint16_t pixBufIndex = LED_TO_BITMAP_MAPPING[i];
        
        pixel_buffer[pixBufIndex] = on_off_100_frames_val;
        pixel_buffer[pixBufIndex + 1] = on_off_100_frames_val;
        pixel_buffer[pixBufIndex + 2] = on_off_100_frames_val;
    }
    on_off_100_frames_n++;
    if (on_off_100_frames_n == 100) {
        on_off_100_frames_n = 0;
        on_off_100_frames_val = 255 - on_off_100_frames_val;
    }
    #endif
}

typedef struct {
    int32_t position;        // Current position of the matrix column (top-most pixel)
    int32_t length;          // Length in pixels
    int64_t next_update_us;  // Next update time in us
    int64_t update_delay_us; // Delay between updates in us
} matrix_column_t;
static matrix_column_t* matrix_columns;
static int64_t matrix_prev_t = 0;
void bitmap_generator_matrix(int64_t t, uint16_t speed, color_rgb_u8_t base_color, color_rgb_u8_t lead_color) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    static int initialized = 0;

    if (!initialized) {
        matrix_columns = malloc(sizeof(matrix_column_t) * frame_width);
        for (uint16_t i = 0; i < frame_width; i++) {
            matrix_columns[i].position = rand() % frame_height;
            matrix_columns[i].length = (rand() % (frame_height - 10 - (frame_height / 2))) + (frame_height / 2); // Random length
            matrix_columns[i].next_update_us = rand() % 200000; // Random initial delay in us
            matrix_columns[i].update_delay_us = (rand() % 90000) + 10000; // Speed with slight variation, 10000 ... 100000
        }
        matrix_prev_t = t;
        initialized = 1;
    }

    for (uint16_t i = 0; i < MAPPING_LENGTH; i++) {
        uint16_t pixBufIndex = LED_TO_BITMAP_MAPPING[i];
        uint16_t x = pixBufIndex / (frame_height * 3);
        uint16_t y = pixBufIndex % (frame_height * 3) / 3;

        // Check if the current matrix column should be updated
        if (t >= matrix_columns[x].next_update_us) {
            // Calculate the new position for the current column
            matrix_columns[x].position++;
            if (matrix_columns[x].position >= (int32_t)frame_height) matrix_columns[x].position = -matrix_columns[x].length;
            fx52_12_t delay_fx = FX52_12(matrix_columns[x].update_delay_us);
            fx20_12_t delay_factor_fx = FX20_12(10) / (speed + 1);
            fx52_12_t next_update_fx = FX52_12(t) + UNFX52_12(delay_fx * delay_factor_fx);
            matrix_columns[x].next_update_us = UNFX52_12_ROUND(next_update_fx);
        }

        // Determine if the current y position is within the "raining" character range
        if (y >= matrix_columns[x].position && y < matrix_columns[x].position + matrix_columns[x].length) {
            // Calculate the position of this pixel relative to the column (0 = top-most pixel)
            uint16_t rel_pos = y - matrix_columns[x].position;
            if (rel_pos == matrix_columns[x].length - 1) {
                // Set the bottom-most pixel to lead color
                pixel_buffer[pixBufIndex] = lead_color.r;
                pixel_buffer[pixBufIndex + 1] = lead_color.g;
                pixel_buffer[pixBufIndex + 2] = lead_color.b;
            } else {
                // Set the pixel to base color, fading to dark towards the top
                color_hsv_fx20_12_t hsv_fx = rgb_u8_to_hsv_fx20_12(base_color);
                fx20_12_t scale_fx = FX20_12(rel_pos) / matrix_columns[x].length;
                hsv_fx.v = UNFX20_12(hsv_fx.v * scale_fx);
                color_rgb_u8_t rgb = hsv_fx20_12_to_rgb_u8(hsv_fx);
                pixel_buffer[pixBufIndex] = rgb.r;
                pixel_buffer[pixBufIndex + 1] = rgb.g;
                pixel_buffer[pixBufIndex + 2] = rgb.b;
            }
        } else {
            // Set the pixel to black (background)
            pixel_buffer[pixBufIndex] = 0;
            pixel_buffer[pixBufIndex + 1] = 0;
            pixel_buffer[pixBufIndex + 2] = 0;
        }
    }

    // Update the previous timestamp
    matrix_prev_t = t;
    #endif
}

void bitmap_generator_plasma(int64_t t, uint16_t speed, uint16_t scale, uint8_t s, uint8_t v) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    fx20_12_t normalized_scale_fx = FX20_12(scale) / 100;
    fx20_12_t normalized_s_fx = FX20_12(s) / 255;
    fx20_12_t normalized_v_fx = FX20_12(v) / 255;

    for (uint16_t i = 0; i < MAPPING_LENGTH; i++) {
        uint16_t pixBufIndex = LED_TO_BITMAP_MAPPING[i];
        uint16_t x = pixBufIndex / (frame_height * 3);
        uint16_t y = pixBufIndex % (frame_height * 3) / 3;
        
        int32_t x_units = (x * 0x4000) / frame_width - 0x2000;
        int32_t y_units = (y * 0x4000) / frame_height - 0x2000;
        
        x_units = x_units * scale / 10;
        y_units = y_units * scale / 10;
        
        fx52_12_t t_sec_fx = FX52_12(t) / 1000000; // It's a UNIX timestamp, so it needs more bits
        fx52_12_t t_speed_fx = t_sec_fx * 1000 * speed;
        
        fx20_12_t sin_x_fx = sin_i16_to_fx20_12(x_units + UNFX52_12(t_speed_fx * 5 / 10));
        fx20_12_t sin_2x_fx = sin_i16_to_fx20_12(x_units * 2 + UNFX52_12(t_speed_fx * 6 / 10));
        fx20_12_t sin_y_fx = sin_i16_to_fx20_12(y_units + UNFX52_12(t_speed_fx * 7 / 10));
        fx20_12_t sin_2y_fx = sin_i16_to_fx20_12(y_units * 2 + UNFX52_12(t_speed_fx * 8 / 10));
        fx20_12_t sin_circ_fx = sin_i16_to_fx20_12(UNFX20_12_ROUND(sqrt_i32_to_fx20_12(x_units*x_units + y_units*y_units)) * 2 + UNFX52_12(t_speed_fx * 9 / 10));
        
        fx20_12_t sin_combined_fx = (sin_x_fx + sin_2x_fx + sin_y_fx + sin_2y_fx + sin_circ_fx) / 5;
        
        fx20_12_t hue_fx = (sin_combined_fx + FX20_12(1)) * 180;
        
        color_hsv_fx20_12_t hsv_fx;
        hsv_fx.h = hue_fx % FX20_12(360);
        hsv_fx.s = normalized_s_fx;
        hsv_fx.v = normalized_v_fx;

        // Get the color for the current segment
        color_rgb_u8_t rgb_u8 = hsv_fx20_12_to_rgb_u8(hsv_fx);
        pixel_buffer[pixBufIndex] = rgb_u8.r;
        pixel_buffer[pixBufIndex + 1] = rgb_u8.g;
        pixel_buffer[pixBufIndex + 2] = rgb_u8.b;
    }
    #endif
}

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

EXPORT void solid_single(int64_t t, uint8_t r, uint8_t g, uint8_t b) {
    color_rgb_u8_t color = { r, g, b };
    bitmap_generator_solid_single(t, color);
}

EXPORT void rainbow_t(int64_t t, uint16_t speed) {
    bitmap_generator_rainbow_t(t, speed);
}

EXPORT void rainbow_gradient(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, uint8_t s, uint8_t v) {
    bitmap_generator_rainbow_gradient(t, speed, angle, scale, s, v);
}

EXPORT void hard_gradient_3(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2, uint8_t r3, uint8_t g3, uint8_t b3) {
    color_rgb_u8_t color1 = { r1, g1, b1 };
    color_rgb_u8_t color2 = { r2, g2, b2 };
    color_rgb_u8_t color3 = { r3, g3, b3 };
    bitmap_generator_hard_gradient_3(t, speed, angle, scale, color1, color2, color3);
}

EXPORT void on_off_100_frames(int64_t t) {
    bitmap_generator_on_off_100_frames(t);
}

EXPORT void matrix(int64_t t, uint16_t speed, uint8_t base_r, uint8_t base_g, uint8_t base_b, uint8_t lead_r, uint8_t lead_g, uint8_t lead_b) {
    color_rgb_u8_t base_color = { base_r, base_g, base_b };
    color_rgb_u8_t lead_color = { lead_r, lead_g, lead_b };
    bitmap_generator_matrix(t, speed, base_color, lead_color);
}

EXPORT void plasma(int64_t t, uint16_t speed, uint16_t scale, uint8_t s, uint8_t v) {
    bitmap_generator_plasma(t, speed, scale, s, v);
}

EXPORT void set_pixel_buffer(uint8_t* buffer, uint32_t size, uint32_t width, uint32_t height) {
    pixel_buffer = buffer;
    pixel_buffer_size = size;
    frame_width = width;
    frame_height = height;
}

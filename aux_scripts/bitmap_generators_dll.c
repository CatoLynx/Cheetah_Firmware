#include <stdint.h>
#include <math.h>

#include "char_16seg_mapping.h"

typedef struct {
    double r;       // a fraction between 0 and 1
    double g;       // a fraction between 0 and 1
    double b;       // a fraction between 0 and 1
} color_rgb_t;

typedef struct {
    double h;       // angle in degrees
    double s;       // a fraction between 0 and 1
    double v;       // a fraction between 0 and 1
} color_hsv_t;

uint8_t* pixel_buffer;
uint32_t pixel_buffer_size;
uint32_t frame_width;
uint32_t frame_height;

// Forward declaration of the conversion function
color_rgb_t hsv2rgb(color_hsv_t hsv);

void bitmap_generator_solid_single(int64_t t, color_rgb_t color) {
    uint8_t red = color.r * 255;
    uint8_t green = color.g * 255;
    uint8_t blue = color.b * 255;
    for (uint16_t i = 0; i < MAPPING_LENGTH; i++) {
        uint16_t pixBufIndex = LED_TO_BITMAP_MAPPING[i];
        
        pixel_buffer[pixBufIndex] = red;
        pixel_buffer[pixBufIndex + 1] = green;
        pixel_buffer[pixBufIndex + 2] = blue;
    }
}

void bitmap_generator_rainbow_t(int64_t t, uint16_t speed) {
    color_hsv_t calcColor_hsv;
    calcColor_hsv.h = (double)speed * (double)t / 1000000.0;
    calcColor_hsv.h = fmod(calcColor_hsv.h, 360.0);
    calcColor_hsv.s = 1.0;
    calcColor_hsv.v = 1.0;
    color_rgb_t color = hsv2rgb(calcColor_hsv);
    bitmap_generator_solid_single(t, color);
}

void bitmap_generator_rainbow_gradient(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, uint8_t s, uint8_t v) {
    double angle_degrees = (double)angle;
    double angle_radians = angle_degrees * M_PI / 180.0;
    double sin_angle = sin(angle_radians);
    double cos_angle = cos(angle_radians);
    double normalized_scale = (double)scale / 100.0;
    double normalized_s = (double)s / 255.0;
    double normalized_v = (double)v / 255.0;

    // Calculate the diagonal length of the frame
    double diagonal_length = sqrt(frame_width * frame_width + frame_height * frame_height);

    for (uint16_t i = 0; i < MAPPING_LENGTH; i++) {
        uint16_t pixBufIndex = LED_TO_BITMAP_MAPPING[i];
        uint16_t x = pixBufIndex / (frame_height * 3);
        uint16_t y = pixBufIndex % (frame_height * 3) / 3;
        
        // Calculate the distance along the gradient direction
        double distance_along_gradient = x * cos_angle + y * sin_angle;
        // Normalize the distance by the diagonal length to get a value between 0 and 1
        double normalized_distance = distance_along_gradient / diagonal_length;
        // Scale the normalized distance to 360 degrees, add 1 to ensure we are positive
        double offset = (normalized_distance + 1.0) * 360.0;
        // Apply manual scaling
        offset *= normalized_scale;
        
        color_hsv_t hsv;
        hsv.h = fmod((double)speed * normalized_scale /*To compensate for scaling*/ * (double)t / 1000000.0 + offset, 360.0);
        hsv.s = normalized_s;
        hsv.v = normalized_v;
        color_rgb_t rgb = hsv2rgb(hsv);
        pixel_buffer[pixBufIndex] = rgb.r * 255;
        pixel_buffer[pixBufIndex + 1] = rgb.g * 255;
        pixel_buffer[pixBufIndex + 2] = rgb.b * 255;
    }
}

void bitmap_generator_hard_gradient(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, uint16_t numColors, color_rgb_t* colors) {
    double angle_degrees = (double)angle;
    double angle_radians = angle_degrees * M_PI / 180.0;
    double sin_angle = sin(angle_radians);
    double cos_angle = cos(angle_radians);
    double normalized_scale = (double)scale / 100.0;

    // Calculate the diagonal length of the frame
    double diagonal_length = sqrt(frame_width * frame_width + frame_height * frame_height);

    // Calculate the effective segment width considering the angle
    double segment_width = diagonal_length / numColors;
    
    for (uint16_t i = 0; i < MAPPING_LENGTH; i++) {
        uint32_t pixBufIndex = LED_TO_BITMAP_MAPPING[i];
        uint16_t x = pixBufIndex / (frame_height * 3);
        uint16_t y = pixBufIndex % (frame_height * 3) / 3;
        
        // Calculate the distance along the gradient direction
        double distance_along_gradient = x * cos_angle + y * sin_angle;
        // Normalize the distance by the diagonal length to get a value between 0 and 1
        double normalized_distance = distance_along_gradient / diagonal_length;
        // Add 1 to ensure we are positive
        double offset = (normalized_distance + 1.0);
        // Apply manual scaling
        offset *= normalized_scale;
        
        double temp = fmod((double)speed / 100.0 * normalized_scale /*To compensate for scaling*/ * (double)t / 1000000.0 + offset, 1.0);
        // Determine the segment index
        uint16_t segment_index = (uint16_t)(temp * numColors) % numColors;

        // Get the color for the current segment
        color_rgb_t color = colors[segment_index];
        pixel_buffer[pixBufIndex] = color.r * 255;
        pixel_buffer[pixBufIndex + 1] = color.g * 255;
        pixel_buffer[pixBufIndex + 2] = color.b * 255;
    }
}

void bitmap_generator_hard_gradient_3(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, color_rgb_t color1, color_rgb_t color2, color_rgb_t color3) {
    color_rgb_t colors[3] = {color1, color2, color3};
    bitmap_generator_hard_gradient(t, speed, angle, scale, 3, colors);
}

color_rgb_t hsv2rgb(color_hsv_t hsv) {
    color_rgb_t rgb;
    double hh, p, q, t, ff;
    long i;

    if (hsv.s <= 0.0) {       // < is bogus, just shuts up warnings
        rgb.r = hsv.v;
        rgb.g = hsv.v;
        rgb.b = hsv.v;
        return rgb;
    }
    hh = hsv.h;
    if (hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = hsv.v * (1.0 - hsv.s);
    q = hsv.v * (1.0 - (hsv.s * ff));
    t = hsv.v * (1.0 - (hsv.s * (1.0 - ff)));

    switch(i) {
    case 0:
        rgb.r = hsv.v;
        rgb.g = t;
        rgb.b = p;
        break;
    case 1:
        rgb.r = q;
        rgb.g = hsv.v;
        rgb.b = p;
        break;
    case 2:
        rgb.r = p;
        rgb.g = hsv.v;
        rgb.b = t;
        break;
    case 3:
        rgb.r = p;
        rgb.g = q;
        rgb.b = hsv.v;
        break;
    case 4:
        rgb.r = t;
        rgb.g = p;
        rgb.b = hsv.v;
        break;
    case 5:
    default:
        rgb.r = hsv.v;
        rgb.g = p;
        rgb.b = q;
        break;
    }
    return rgb;
}

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

EXPORT void solid_single(int64_t t, double r, double g, double b) {
    color_rgb_t color = { r, g, b };
    bitmap_generator_solid_single(t, color);
}

EXPORT void rainbow_t(int64_t t, uint16_t speed) {
    bitmap_generator_rainbow_t(t, speed);
}

EXPORT void rainbow_gradient(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, uint8_t s, uint8_t v) {
    bitmap_generator_rainbow_gradient(t, speed, angle, scale, s, v);
}

EXPORT void hard_gradient_3(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, double r1, double g1, double b1, double r2, double g2, double b2, double r3, double g3, double b3) {
    color_rgb_t color1 = { r1, g1, b1 };
    color_rgb_t color2 = { r2, g2, b2 };
    color_rgb_t color3 = { r3, g3, b3 };
    bitmap_generator_hard_gradient_3(t, speed, angle, scale, color1, color2, color3);
}

EXPORT void set_pixel_buffer(uint8_t* buffer, uint32_t size, uint32_t width, uint32_t height) {
    pixel_buffer = buffer;
    pixel_buffer_size = size;
    frame_width = width;
    frame_height = height;
}

#include "esp_log.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bitmap_generators.h"
#include "i2s_microphone.h"
#include "util_fixed_point.h"
#include "macros.h"
#include "util_buffer.h"
#include "util_generic.h"
#include "util_gpio.h"
#include "math.h"

// TODO: More elegant buffer type gating than just gating the entire function content


#define LOG_TAG "BITMAP-GEN"


#if defined(DISPLAY_HAS_PIXEL_BUFFER)


static uint8_t* pixel_buffer;
static size_t pixel_buffer_size = 0;
static portMUX_TYPE* pixel_buffer_lock = NULL;
static uint16_t frame_width = 0;
static uint16_t frame_height = 0;
static cJSON* current_bitmap_generator = NULL;
static color_rgb_u8_t white = {.r = 255, .g = 255, .b = 255};

// TODO: This is REALLY UGLY and a hack for one specific display.
// I don't have time right now to do it properly, but this can't stay like that.
#define MAPPING_LENGTH 2062
extern const uint16_t LED_TO_BITMAP_MAPPING[MAPPING_LENGTH];

// Order of generators. This is important!
// This order needs to match the order in which the generators
// are output by bitmap_generators_get_available().
enum generator_func {
    NONE = 0,
    SOLID_SINGLE,
    RAINBOW_T,
    RAINBOW_GRADIENT,
    HARD_GRADIENT_2,
    HARD_GRADIENT_3,
    SOFT_GRADIENT_2,
    SOFT_GRADIENT_3,
    ON_OFF_100_FRAMES,
    MATRIX,
    PLASMA,
    PLASMA_2,
    MIC_FFT,
    MIC_AMPLITUDE
};


void bitmap_generators_init(uint8_t* pixBuf, size_t pixBufSize, portMUX_TYPE* pixBufLock, uint16_t frameWidth, uint16_t frameHeight) {
    ESP_LOGI(LOG_TAG, "Initializing bitmap generators");
    pixel_buffer = pixBuf;
    pixel_buffer_size = pixBufSize;
    pixel_buffer_lock = pixBufLock;
    frame_width = frameWidth;
    frame_height = frameHeight;
}

void bitmap_generator_select(cJSON* bitmapGeneratorData) {
    current_bitmap_generator = bitmapGeneratorData;
}

cJSON* bitmap_generators_get_available() {
    cJSON *generator_entry, *params, *param;

    cJSON* json = cJSON_CreateObject();
    cJSON* generators_arr = cJSON_AddArrayToObject(json, "generators");

    // Generator: None (Dummy)
    generator_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(generator_entry, "name", "none");
    params = cJSON_CreateObject();
    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    // Generator: Solid Single
    generator_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(generator_entry, "name", "solid_single");
    params = cJSON_CreateObject();

        // Parameter: Color
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "color", param);

    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    // Generator: Rainbow over time
    generator_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(generator_entry, "name", "rainbow_t");
    params = cJSON_CreateObject();

        // Parameter: Speed
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 10);
        cJSON_AddItemToObject(params, "speed", param);

    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    // Generator: Rainbow gradient
    generator_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(generator_entry, "name", "rainbow_gradient");
    params = cJSON_CreateObject();

        // Parameter: Speed
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 10);
        cJSON_AddItemToObject(params, "speed", param);

        // Parameter: Angle
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 359);
        cJSON_AddNumberToObject(param, "value", 0);
        cJSON_AddItemToObject(params, "angle", param);

        // Parameter: Scale
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 1000);
        cJSON_AddNumberToObject(param, "value", 100);
        cJSON_AddItemToObject(params, "scale", param);

        // Parameter: Saturation
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 255);
        cJSON_AddNumberToObject(param, "value", 255);
        cJSON_AddItemToObject(params, "saturation", param);

        // Parameter: Value
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 255);
        cJSON_AddNumberToObject(param, "value", 255);
        cJSON_AddItemToObject(params, "value", param);

    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    // Generator: Hard gradient with 2 colors
    generator_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(generator_entry, "name", "hard_gradient_2");
    params = cJSON_CreateObject();

        // Parameter: Speed
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 10);
        cJSON_AddItemToObject(params, "speed", param);

        // Parameter: Angle
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 359);
        cJSON_AddNumberToObject(param, "value", 0);
        cJSON_AddItemToObject(params, "angle", param);

        // Parameter: Scale
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 1000);
        cJSON_AddNumberToObject(param, "value", 100);
        cJSON_AddItemToObject(params, "scale", param);

        // Parameter: Color 1
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "color1", param);

        // Parameter: Color 2
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "color2", param);

    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    // Generator: Hard gradient with 3 colors
    generator_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(generator_entry, "name", "hard_gradient_3");
    params = cJSON_CreateObject();

        // Parameter: Speed
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 10);
        cJSON_AddItemToObject(params, "speed", param);

        // Parameter: Angle
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 359);
        cJSON_AddNumberToObject(param, "value", 0);
        cJSON_AddItemToObject(params, "angle", param);

        // Parameter: Scale
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 1000);
        cJSON_AddNumberToObject(param, "value", 100);
        cJSON_AddItemToObject(params, "scale", param);

        // Parameter: Color 1
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "color1", param);

        // Parameter: Color 2
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "color2", param);

        // Parameter: Color 3
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "color3", param);

    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    // Generator: Soft gradient with 2 colors
    generator_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(generator_entry, "name", "soft_gradient_2");
    params = cJSON_CreateObject();

        // Parameter: Speed
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 10);
        cJSON_AddItemToObject(params, "speed", param);

        // Parameter: Angle
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 359);
        cJSON_AddNumberToObject(param, "value", 0);
        cJSON_AddItemToObject(params, "angle", param);

        // Parameter: Scale
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 1000);
        cJSON_AddNumberToObject(param, "value", 100);
        cJSON_AddItemToObject(params, "scale", param);

        // Parameter: Color 1
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "color1", param);

        // Parameter: Color 2
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "color2", param);

    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    // Generator: Soft gradient with 3 colors
    generator_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(generator_entry, "name", "soft_gradient_3");
    params = cJSON_CreateObject();

        // Parameter: Speed
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 10);
        cJSON_AddItemToObject(params, "speed", param);

        // Parameter: Angle
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 359);
        cJSON_AddNumberToObject(param, "value", 0);
        cJSON_AddItemToObject(params, "angle", param);

        // Parameter: Scale
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 1000);
        cJSON_AddNumberToObject(param, "value", 100);
        cJSON_AddItemToObject(params, "scale", param);

        // Parameter: Color 1
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "color1", param);

        // Parameter: Color 2
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "color2", param);

        // Parameter: Color 3
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "color3", param);

    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    // Generator: On/Off every 100 frames
    generator_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(generator_entry, "name", "on_off_100_frames");
    params = cJSON_CreateObject();
    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    // Generator: Matrix
    generator_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(generator_entry, "name", "matrix");
    params = cJSON_CreateObject();

        // Parameter: Speed
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 10);
        cJSON_AddItemToObject(params, "speed", param);

        // Parameter: Base Color
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "base_color", param);

        // Parameter: Lead Color
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "lead_color", param);

    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    // Generator: Plasma
    generator_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(generator_entry, "name", "plasma");
    params = cJSON_CreateObject();

        // Parameter: Speed
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 10);
        cJSON_AddItemToObject(params, "speed", param);

        // Parameter: Scale
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 1000);
        cJSON_AddNumberToObject(param, "value", 10);
        cJSON_AddItemToObject(params, "scale", param);

        // Parameter: Saturation
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 255);
        cJSON_AddNumberToObject(param, "value", 255);
        cJSON_AddItemToObject(params, "saturation", param);

        // Parameter: Value
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 255);
        cJSON_AddNumberToObject(param, "value", 255);
        cJSON_AddItemToObject(params, "value", param);

    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    // Generator: Plasma 2
    generator_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(generator_entry, "name", "plasma_2");
    params = cJSON_CreateObject();

        // Parameter: Speed
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 10);
        cJSON_AddItemToObject(params, "speed", param);

        // Parameter: Scale
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 1000);
        cJSON_AddNumberToObject(param, "value", 10);
        cJSON_AddItemToObject(params, "scale", param);

        // Parameter: Color 1
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "color1", param);

        // Parameter: Color 2
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "color2", param);

    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    // Generator: 1bpp Microphone FFT
    generator_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(generator_entry, "name", "1bpp_mic_fft");
    params = cJSON_CreateObject();

        // Parameter: Logarithmic
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "checkbox");
        cJSON_AddBoolToObject(param, "checked", 0);
        cJSON_AddItemToObject(params, "logarithmic", param);

        // Parameter: Lin/Log Factor
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 50);
        cJSON_AddItemToObject(params, "lin_log_factor", param);

        // Parameter: Line only
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "checkbox");
        cJSON_AddBoolToObject(param, "checked", 0);
        cJSON_AddItemToObject(params, "line_only", param);

        // Parameter: Min. Normalization Factor (* 0.1)
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 10);
        cJSON_AddNumberToObject(param, "max", 5000);
        cJSON_AddNumberToObject(param, "value", 10);
        cJSON_AddItemToObject(params, "min_norm_factor", param);

        // Parameter: Max. Normalization Factor (* 0.1)
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 5000);
        cJSON_AddNumberToObject(param, "value", 5000);
        cJSON_AddItemToObject(params, "max_norm_factor", param);

        // Parameter: Normalization Factor Increase (* 0.1 ^ -1)
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 10000);
        cJSON_AddNumberToObject(param, "value", 5120);
        cJSON_AddItemToObject(params, "norm_factor_increase", param);

        // Parameter: Normalization Factor Decrease (* 0.1 ^ -1)
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 10000);
        cJSON_AddNumberToObject(param, "value", 160);
        cJSON_AddItemToObject(params, "norm_factor_decrease", param);

        // Parameter: Bin Width
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 255);
        cJSON_AddNumberToObject(param, "value", 1);
        cJSON_AddItemToObject(params, "bin_width", param);

    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    // Generator: 1bpp Microphone Amplitude
    generator_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(generator_entry, "name", "1bpp_mic_amplitude");
    params = cJSON_CreateObject();

        // Parameter: Min. Normalization Factor (* 0.1)
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 10);
        cJSON_AddNumberToObject(param, "max", 5000);
        cJSON_AddNumberToObject(param, "value", 10);
        cJSON_AddItemToObject(params, "min_norm_factor", param);

        // Parameter: Max. Normalization Factor (* 0.1)
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 5000);
        cJSON_AddNumberToObject(param, "value", 5000);
        cJSON_AddItemToObject(params, "max_norm_factor", param);

        // Parameter: Normalization Factor Increase (* 0.1 ^ -1)
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 10000);
        cJSON_AddNumberToObject(param, "value", 5120);
        cJSON_AddItemToObject(params, "norm_factor_increase", param);

        // Parameter: Normalization Factor Decrease (* 0.1 ^ -1)
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 10000);
        cJSON_AddNumberToObject(param, "value", 160);
        cJSON_AddItemToObject(params, "norm_factor_decrease", param);
    
    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    return json;
}

void bitmap_generator_none(int64_t t) {
    
}

void bitmap_generator_solid_single(int64_t t, color_rgb_u8_t color) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    taskENTER_CRITICAL(pixel_buffer_lock);
    for (uint16_t i = 0; i < MAPPING_LENGTH; i++) {
        uint16_t pixBufIndex = LED_TO_BITMAP_MAPPING[i];
        
        pixel_buffer[pixBufIndex] = color.r;
        pixel_buffer[pixBufIndex + 1] = color.g;
        pixel_buffer[pixBufIndex + 2] = color.b;
    }
    taskEXIT_CRITICAL(pixel_buffer_lock);
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

    taskENTER_CRITICAL(pixel_buffer_lock);
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
    taskEXIT_CRITICAL(pixel_buffer_lock);
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
    
    taskENTER_CRITICAL(pixel_buffer_lock);
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
    taskEXIT_CRITICAL(pixel_buffer_lock);
    #endif
}

void bitmap_generator_hard_gradient_2(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, color_rgb_u8_t color1, color_rgb_u8_t color2) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    color_rgb_u8_t colors[2] = {color1, color2};
    bitmap_generator_hard_gradient(t, speed, angle, scale, 2, colors);
    #endif
}

void bitmap_generator_hard_gradient_3(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, color_rgb_u8_t color1, color_rgb_u8_t color2, color_rgb_u8_t color3) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    color_rgb_u8_t colors[3] = {color1, color2, color3};
    bitmap_generator_hard_gradient(t, speed, angle, scale, 3, colors);
    #endif
}

void bitmap_generator_soft_gradient(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, uint16_t numColors, color_rgb_u8_t* colors) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    int32_t angle_units = (angle * 0x8000) / 360;
    fx20_12_t sin_angle_fx = sin_i16_to_fx20_12(angle_units);
    fx20_12_t cos_angle_fx = cos_i16_to_fx20_12(angle_units);
    fx20_12_t normalized_scale_fx = FX20_12(scale) / 100;

    // Calculate the diagonal length of the frame
    fx20_12_t diagonal_length_fx = sqrt_i32_to_fx20_12(frame_width * frame_width + frame_height * frame_height);
    
    taskENTER_CRITICAL(pixel_buffer_lock);
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
        fx20_12_t segment_index_fx = temp_fx * numColors;
        uint16_t segment_index_base = (uint16_t)UNFX20_12(segment_index_fx) % numColors;
        uint16_t next_segment_index = (segment_index_base + 1) % numColors;
        fx20_12_t segment_fraction_fx = segment_index_fx % FX20_12(1);

        // Get the color for the current segment
        color_rgb_u8_t color1 = colors[segment_index_base];
        color_rgb_u8_t color2 = colors[next_segment_index];
        pixel_buffer[pixBufIndex] = interpolate_fx20_12_i32(segment_fraction_fx, color1.r, color2.r);
        pixel_buffer[pixBufIndex + 1] = interpolate_fx20_12_i32(segment_fraction_fx, color1.g, color2.g);
        pixel_buffer[pixBufIndex + 2] = interpolate_fx20_12_i32(segment_fraction_fx, color1.b, color2.b);
    }
    taskEXIT_CRITICAL(pixel_buffer_lock);
    #endif
}

void bitmap_generator_soft_gradient_2(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, color_rgb_u8_t color1, color_rgb_u8_t color2) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    color_rgb_u8_t colors[2] = {color1, color2};
    bitmap_generator_soft_gradient(t, speed, angle, scale, 2, colors);
    #endif
}

void bitmap_generator_soft_gradient_3(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, color_rgb_u8_t color1, color_rgb_u8_t color2, color_rgb_u8_t color3) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    color_rgb_u8_t colors[3] = {color1, color2, color3};
    bitmap_generator_soft_gradient(t, speed, angle, scale, 3, colors);
    #endif
}

void bitmap_generator_on_off_100_frames(int64_t t) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    static uint8_t on_off_100_frames_n = 0;
    static uint8_t on_off_100_frames_val = 255;

    taskENTER_CRITICAL(pixel_buffer_lock);
    for (uint16_t i = 0; i < MAPPING_LENGTH; i++) {
        uint16_t pixBufIndex = LED_TO_BITMAP_MAPPING[i];
        
        pixel_buffer[pixBufIndex] = on_off_100_frames_val;
        pixel_buffer[pixBufIndex + 1] = on_off_100_frames_val;
        pixel_buffer[pixBufIndex + 2] = on_off_100_frames_val;
    }
    taskEXIT_CRITICAL(pixel_buffer_lock);
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

    taskENTER_CRITICAL(pixel_buffer_lock);
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
    taskEXIT_CRITICAL(pixel_buffer_lock);

    // Update the previous timestamp
    matrix_prev_t = t;
    #endif
}

void bitmap_generator_plasma(int64_t t, uint16_t speed, uint16_t scale, uint8_t s, uint8_t v) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    fx20_12_t normalized_s_fx = FX20_12(s) / 255;
    fx20_12_t normalized_v_fx = FX20_12(v) / 255;

    taskENTER_CRITICAL(pixel_buffer_lock);
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
    taskEXIT_CRITICAL(pixel_buffer_lock);
    #endif
}

void bitmap_generator_plasma_2(int64_t t, uint16_t speed, uint16_t scale, color_rgb_u8_t color1, color_rgb_u8_t color2) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)

    taskENTER_CRITICAL(pixel_buffer_lock);
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
        
        fx20_12_t gradient_pos_fx = (sin_combined_fx + FX20_12(1)) / 2;

        // Get the color for the current segment
        pixel_buffer[pixBufIndex] = interpolate_fx20_12_i32(gradient_pos_fx, color1.r, color2.r);
        pixel_buffer[pixBufIndex + 1] = interpolate_fx20_12_i32(gradient_pos_fx, color1.g, color2.g);
        pixel_buffer[pixBufIndex + 2] = interpolate_fx20_12_i32(gradient_pos_fx, color1.b, color2.b);
    }
    taskEXIT_CRITICAL(pixel_buffer_lock);
    #endif
}

static float fft_bins[DISPLAY_FRAME_WIDTH_PIXEL] = { 0.0f };
static int8_t fft_columns[DISPLAY_FRAME_WIDTH_PIXEL] = { 0 };
void bitmap_generator_1bpp_mic_fft(int64_t t, uint8_t logarithmic, uint8_t lin_log_factor, uint8_t lineOnly, uint16_t minNormFactor, uint16_t maxNormFactor, uint16_t normFactorChangeIncrease, uint16_t normFactorChangeDecrease, uint8_t binWidth) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_1BPP)
    if (binWidth == 0) binWidth = 1;
    uint16_t numBins = DISPLAY_FRAME_WIDTH_PIXEL / binWidth;
    i2s_mic_get_fft_bins(fft_bins, numBins, logarithmic, (float)lin_log_factor / 100.0f, minNormFactor * 0.1f, maxNormFactor * 0.1f, 1 / (normFactorChangeIncrease * 0.1f), 1 / (normFactorChangeDecrease * 0.1f));

    taskENTER_CRITICAL(pixel_buffer_lock);
    uint8_t bass_count = 0;
    for (uint16_t binIdx = 0; binIdx < numBins; binIdx++) {
        int8_t v = (uint8_t)((fft_bins[binIdx] - 130.0f) * (8 - 0) / (200.0f - 130.0f) + 0); // TODO: Get rid of magic numbers
        if (v < 0) v = 0; // TODO: Adjust for display height
        if (v > 8) v = 8; // TODO: Adjust for display height
        int8_t prev_col = fft_columns[binIdx];
        
        int8_t col = 0;
        if (v >= prev_col) col = v; // Immediately set if new amplitude is greater
        if (v < prev_col && prev_col > 0) col = prev_col - 1; // Otherwise allow 1 pixel decrease per frame
        fft_columns[binIdx] = col;

        for (uint8_t i = 0; i < binWidth; i++) {
            // TODO: Adjust for display height
            if (lineOnly) {
                pixel_buffer[binIdx * binWidth + i] = (0x0100 >> (col)) & 0xFF;
            } else {
                pixel_buffer[binIdx * binWidth + i] = (0xFF00 >> (col)) & 0xFF;
            }
        }

        // TODO: Replace this fucked up shit with actual beat detection or something
        // It counts amplitudes in a specific area
        if (binIdx < 8) {
            // Max. count: 8 * 2 = 16
            if (col >= 7) bass_count += col - 6;
        }
    }
    // TODO: NO no no no no CRUDE BAD aaa
    // Stop indicator kinda mirrors bass level
    gpio_set(23, (bass_count >= 4), 0);
    taskEXIT_CRITICAL(pixel_buffer_lock);
    #endif
}

static uint16_t amplitude_history[DISPLAY_FRAME_WIDTH_PIXEL] = { 0 };
static uint16_t amplitude_history_start = 0;
void bitmap_generator_1bpp_mic_amplitude(int64_t t, uint16_t minNormFactor, uint16_t maxNormFactor, uint16_t normFactorChangeIncrease, uint16_t normFactorChangeDecrease) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_1BPP)
    uint16_t numBins = 10;
    i2s_mic_get_fft_bins(fft_bins, numBins, false, 0.5f, minNormFactor * 0.1f, maxNormFactor * 0.1f, 1 / (normFactorChangeIncrease * 0.1f), 1 / (normFactorChangeDecrease * 0.1f));
    int32_t amplitude = 0;
    for (uint8_t i = 0; i < numBins; i++) {
        amplitude += fft_bins[i];
    }
    amplitude /= numBins;
    uint16_t amplitudePixels = (uint16_t)(amplitude - 130.0f) * DISPLAY_FRAME_HEIGHT_PIXEL / (200.0f - 130.0f); // TODO: Get rid of magic numbers
    amplitude_history[amplitude_history_start++] = amplitudePixels;
    amplitude_history_start %= DISPLAY_FRAME_WIDTH_PIXEL;

    taskENTER_CRITICAL(pixel_buffer_lock);
    for (uint16_t x = 0; x < DISPLAY_FRAME_WIDTH_PIXEL; x++) {
        // TODO: Adjust for display height
        uint16_t histIdx = (amplitude_history_start + x) % DISPLAY_FRAME_WIDTH_PIXEL;
        pixel_buffer[x] = (0xFF00 >> (amplitude_history[histIdx])) & 0xFF;
    }
    taskEXIT_CRITICAL(pixel_buffer_lock);
    #endif
}

static color_rgb_u8_t _color_rgb_u8_from_json(cJSON* json, color_rgb_u8_t fallback) {
    color_rgb_u8_t color;
    cJSON* r_field = cJSON_GetObjectItem(json, "r");
    if (!cJSON_IsNumber(r_field)) return fallback;
    color.r = cJSON_GetNumberValue(r_field);
    cJSON* g_field = cJSON_GetObjectItem(json, "g");
    if (!cJSON_IsNumber(g_field)) return fallback;
    color.g = cJSON_GetNumberValue(g_field);
    cJSON* b_field = cJSON_GetObjectItem(json, "b");
    if (!cJSON_IsNumber(b_field)) return fallback;
    color.b = cJSON_GetNumberValue(b_field);
    return color;
}

void bitmap_generator_current(int64_t t) {
    if (current_bitmap_generator == NULL) return;

    cJSON* generator_id_field = cJSON_GetObjectItem(current_bitmap_generator, "generator");
    if (!cJSON_IsNumber(generator_id_field)) return;
    enum generator_func generatorId = (enum generator_func)cJSON_GetNumberValue(generator_id_field);
    ESP_LOGV(LOG_TAG, "generator=%p generatorId=%u", current_bitmap_generator, generatorId);

    cJSON* params = cJSON_GetObjectItem(current_bitmap_generator, "params");
    if (!cJSON_IsObject(params)) return;

    switch (generatorId) {
        case NONE: {
            bitmap_generator_none(t);
            return;
        }

        case SOLID_SINGLE: {
            cJSON* color_obj = cJSON_GetObjectItem(params, "color");
            if (!cJSON_IsObject(color_obj)) return;
            color_rgb_u8_t color = _color_rgb_u8_from_json(color_obj, white);
            bitmap_generator_solid_single(t, color);
            return;
        }

        case RAINBOW_T: {
            cJSON* speed_field = cJSON_GetObjectItem(params, "speed");
            if (!cJSON_IsNumber(speed_field)) return;
            uint16_t speed = (uint16_t)cJSON_GetNumberValue(speed_field);
            bitmap_generator_rainbow_t(t, speed);
            return;
        }

        case RAINBOW_GRADIENT: {
            cJSON* speed_field = cJSON_GetObjectItem(params, "speed");
            if (!cJSON_IsNumber(speed_field)) return;
            uint16_t speed = (uint16_t)cJSON_GetNumberValue(speed_field);
            cJSON* angle_field = cJSON_GetObjectItem(params, "angle");
            if (!cJSON_IsNumber(angle_field)) return;
            uint16_t angle = (uint16_t)cJSON_GetNumberValue(angle_field);
            cJSON* scale_field = cJSON_GetObjectItem(params, "scale");
            if (!cJSON_IsNumber(scale_field)) return;
            uint16_t scale = (uint16_t)cJSON_GetNumberValue(scale_field);
            cJSON* saturation_field = cJSON_GetObjectItem(params, "saturation");
            if (!cJSON_IsNumber(saturation_field)) return;
            uint8_t saturation = (uint8_t)cJSON_GetNumberValue(saturation_field);
            cJSON* value_field = cJSON_GetObjectItem(params, "value");
            if (!cJSON_IsNumber(value_field)) return;
            uint8_t value = (uint8_t)cJSON_GetNumberValue(value_field);
            bitmap_generator_rainbow_gradient(t, speed, angle, scale, saturation, value);
            return;
        }

        case HARD_GRADIENT_2: {
            cJSON* speed_field = cJSON_GetObjectItem(params, "speed");
            if (!cJSON_IsNumber(speed_field)) return;
            uint16_t speed = (uint16_t)cJSON_GetNumberValue(speed_field);
            cJSON* angle_field = cJSON_GetObjectItem(params, "angle");
            if (!cJSON_IsNumber(angle_field)) return;
            uint16_t angle = (uint16_t)cJSON_GetNumberValue(angle_field);
            cJSON* scale_field = cJSON_GetObjectItem(params, "scale");
            if (!cJSON_IsNumber(scale_field)) return;
            uint16_t scale = (uint16_t)cJSON_GetNumberValue(scale_field);
            cJSON* color1_obj = cJSON_GetObjectItem(params, "color1");
            if (!cJSON_IsObject(color1_obj)) return;
            color_rgb_u8_t color1 = _color_rgb_u8_from_json(color1_obj, white);
            cJSON* color2_obj = cJSON_GetObjectItem(params, "color2");
            if (!cJSON_IsObject(color2_obj)) return;
            color_rgb_u8_t color2 = _color_rgb_u8_from_json(color2_obj, white);
            bitmap_generator_hard_gradient_2(t, speed, angle, scale, color1, color2);
            return;
        }

        case HARD_GRADIENT_3: {
            cJSON* speed_field = cJSON_GetObjectItem(params, "speed");
            if (!cJSON_IsNumber(speed_field)) return;
            uint16_t speed = (uint16_t)cJSON_GetNumberValue(speed_field);
            cJSON* angle_field = cJSON_GetObjectItem(params, "angle");
            if (!cJSON_IsNumber(angle_field)) return;
            uint16_t angle = (uint16_t)cJSON_GetNumberValue(angle_field);
            cJSON* scale_field = cJSON_GetObjectItem(params, "scale");
            if (!cJSON_IsNumber(scale_field)) return;
            uint16_t scale = (uint16_t)cJSON_GetNumberValue(scale_field);
            cJSON* color1_obj = cJSON_GetObjectItem(params, "color1");
            if (!cJSON_IsObject(color1_obj)) return;
            color_rgb_u8_t color1 = _color_rgb_u8_from_json(color1_obj, white);
            cJSON* color2_obj = cJSON_GetObjectItem(params, "color2");
            if (!cJSON_IsObject(color2_obj)) return;
            color_rgb_u8_t color2 = _color_rgb_u8_from_json(color2_obj, white);
            cJSON* color3_obj = cJSON_GetObjectItem(params, "color3");
            if (!cJSON_IsObject(color3_obj)) return;
            color_rgb_u8_t color3 = _color_rgb_u8_from_json(color3_obj, white);
            bitmap_generator_hard_gradient_3(t, speed, angle, scale, color1, color2, color3);
            return;
        }

        case SOFT_GRADIENT_2: {
            cJSON* speed_field = cJSON_GetObjectItem(params, "speed");
            if (!cJSON_IsNumber(speed_field)) return;
            uint16_t speed = (uint16_t)cJSON_GetNumberValue(speed_field);
            cJSON* angle_field = cJSON_GetObjectItem(params, "angle");
            if (!cJSON_IsNumber(angle_field)) return;
            uint16_t angle = (uint16_t)cJSON_GetNumberValue(angle_field);
            cJSON* scale_field = cJSON_GetObjectItem(params, "scale");
            if (!cJSON_IsNumber(scale_field)) return;
            uint16_t scale = (uint16_t)cJSON_GetNumberValue(scale_field);
            cJSON* color1_obj = cJSON_GetObjectItem(params, "color1");
            if (!cJSON_IsObject(color1_obj)) return;
            color_rgb_u8_t color1 = _color_rgb_u8_from_json(color1_obj, white);
            cJSON* color2_obj = cJSON_GetObjectItem(params, "color2");
            if (!cJSON_IsObject(color2_obj)) return;
            color_rgb_u8_t color2 = _color_rgb_u8_from_json(color2_obj, white);
            bitmap_generator_soft_gradient_2(t, speed, angle, scale, color1, color2);
            return;
        }

        case SOFT_GRADIENT_3: {
            cJSON* speed_field = cJSON_GetObjectItem(params, "speed");
            if (!cJSON_IsNumber(speed_field)) return;
            uint16_t speed = (uint16_t)cJSON_GetNumberValue(speed_field);
            cJSON* angle_field = cJSON_GetObjectItem(params, "angle");
            if (!cJSON_IsNumber(angle_field)) return;
            uint16_t angle = (uint16_t)cJSON_GetNumberValue(angle_field);
            cJSON* scale_field = cJSON_GetObjectItem(params, "scale");
            if (!cJSON_IsNumber(scale_field)) return;
            uint16_t scale = (uint16_t)cJSON_GetNumberValue(scale_field);
            cJSON* color1_obj = cJSON_GetObjectItem(params, "color1");
            if (!cJSON_IsObject(color1_obj)) return;
            color_rgb_u8_t color1 = _color_rgb_u8_from_json(color1_obj, white);
            cJSON* color2_obj = cJSON_GetObjectItem(params, "color2");
            if (!cJSON_IsObject(color2_obj)) return;
            color_rgb_u8_t color2 = _color_rgb_u8_from_json(color2_obj, white);
            cJSON* color3_obj = cJSON_GetObjectItem(params, "color3");
            if (!cJSON_IsObject(color3_obj)) return;
            color_rgb_u8_t color3 = _color_rgb_u8_from_json(color3_obj, white);
            bitmap_generator_soft_gradient_3(t, speed, angle, scale, color1, color2, color3);
            return;
        }

        case ON_OFF_100_FRAMES: {
            bitmap_generator_on_off_100_frames(t);
            return;
        }

        case MATRIX: {
            cJSON* speed_field = cJSON_GetObjectItem(params, "speed");
            if (!cJSON_IsNumber(speed_field)) return;
            uint16_t speed = (uint16_t)cJSON_GetNumberValue(speed_field);
            cJSON* base_color_obj = cJSON_GetObjectItem(params, "base_color");
            if (!cJSON_IsObject(base_color_obj)) return;
            color_rgb_u8_t base_color = _color_rgb_u8_from_json(base_color_obj, white);
            cJSON* lead_color_obj = cJSON_GetObjectItem(params, "lead_color");
            if (!cJSON_IsObject(lead_color_obj)) return;
            color_rgb_u8_t lead_color = _color_rgb_u8_from_json(lead_color_obj, white);
            bitmap_generator_matrix(t, speed, base_color, lead_color);
            return;
        }

        case PLASMA: {
            cJSON* speed_field = cJSON_GetObjectItem(params, "speed");
            if (!cJSON_IsNumber(speed_field)) return;
            uint16_t speed = (uint16_t)cJSON_GetNumberValue(speed_field);
            cJSON* scale_field = cJSON_GetObjectItem(params, "scale");
            if (!cJSON_IsNumber(scale_field)) return;
            uint16_t scale = (uint16_t)cJSON_GetNumberValue(scale_field);
            cJSON* saturation_field = cJSON_GetObjectItem(params, "saturation");
            if (!cJSON_IsNumber(saturation_field)) return;
            uint8_t saturation = (uint8_t)cJSON_GetNumberValue(saturation_field);
            cJSON* value_field = cJSON_GetObjectItem(params, "value");
            if (!cJSON_IsNumber(value_field)) return;
            uint8_t value = (uint8_t)cJSON_GetNumberValue(value_field);
            bitmap_generator_plasma(t, speed, scale, saturation, value);
            return;
        }

        case PLASMA_2: {
            cJSON* speed_field = cJSON_GetObjectItem(params, "speed");
            if (!cJSON_IsNumber(speed_field)) return;
            uint16_t speed = (uint16_t)cJSON_GetNumberValue(speed_field);
            cJSON* scale_field = cJSON_GetObjectItem(params, "scale");
            if (!cJSON_IsNumber(scale_field)) return;
            uint16_t scale = (uint16_t)cJSON_GetNumberValue(scale_field);
            cJSON* color1_obj = cJSON_GetObjectItem(params, "color1");
            if (!cJSON_IsObject(color1_obj)) return;
            color_rgb_u8_t color1 = _color_rgb_u8_from_json(color1_obj, white);
            cJSON* color2_obj = cJSON_GetObjectItem(params, "color2");
            if (!cJSON_IsObject(color2_obj)) return;
            color_rgb_u8_t color2 = _color_rgb_u8_from_json(color2_obj, white);
            bitmap_generator_plasma_2(t, speed, scale, color1, color2);
            return;
        }

        case MIC_FFT: {
            cJSON* line_only_field = cJSON_GetObjectItem(params, "line_only");
            if (!cJSON_IsBool(line_only_field)) return;
            uint8_t line_only = (uint8_t)cJSON_IsTrue(line_only_field);
            cJSON* lin_log_factor_field = cJSON_GetObjectItem(params, "lin_log_factor");
            if (!cJSON_IsNumber(lin_log_factor_field)) return;
            uint8_t lin_log_factor = (uint8_t)cJSON_GetNumberValue(lin_log_factor_field);
            cJSON* logarithmic_field = cJSON_GetObjectItem(params, "logarithmic");
            if (!cJSON_IsBool(logarithmic_field)) return;
            uint8_t logarithmic = (uint8_t)cJSON_IsTrue(logarithmic_field);
            cJSON* min_norm_factor_field = cJSON_GetObjectItem(params, "min_norm_factor");
            if (!cJSON_IsNumber(min_norm_factor_field)) return;
            uint16_t min_norm_factor = (uint16_t)cJSON_GetNumberValue(min_norm_factor_field);
            cJSON* max_norm_factor_field = cJSON_GetObjectItem(params, "max_norm_factor");
            if (!cJSON_IsNumber(max_norm_factor_field)) return;
            uint16_t max_norm_factor = (uint16_t)cJSON_GetNumberValue(max_norm_factor_field);
            cJSON* norm_factor_increase_field = cJSON_GetObjectItem(params, "norm_factor_increase");
            if (!cJSON_IsNumber(norm_factor_increase_field)) return;
            uint16_t norm_factor_increase = (uint16_t)cJSON_GetNumberValue(norm_factor_increase_field);
            cJSON* norm_factor_decrease_field = cJSON_GetObjectItem(params, "norm_factor_decrease");
            if (!cJSON_IsNumber(norm_factor_decrease_field)) return;
            uint16_t norm_factor_decrease = (uint16_t)cJSON_GetNumberValue(norm_factor_decrease_field);
            cJSON* bin_width_field = cJSON_GetObjectItem(params, "bin_width");
            if (!cJSON_IsNumber(bin_width_field)) return;
            uint8_t bin_width = (uint8_t)cJSON_GetNumberValue(bin_width_field);
            bitmap_generator_1bpp_mic_fft(t, logarithmic, lin_log_factor, line_only, min_norm_factor, max_norm_factor, norm_factor_increase, norm_factor_decrease, bin_width);
            return;
        }

        case MIC_AMPLITUDE: {
            cJSON* min_norm_factor_field = cJSON_GetObjectItem(params, "min_norm_factor");
            if (!cJSON_IsNumber(min_norm_factor_field)) return;
            uint16_t min_norm_factor = (uint16_t)cJSON_GetNumberValue(min_norm_factor_field);
            cJSON* max_norm_factor_field = cJSON_GetObjectItem(params, "max_norm_factor");
            if (!cJSON_IsNumber(max_norm_factor_field)) return;
            uint16_t max_norm_factor = (uint16_t)cJSON_GetNumberValue(max_norm_factor_field);
            cJSON* norm_factor_increase_field = cJSON_GetObjectItem(params, "norm_factor_increase");
            if (!cJSON_IsNumber(norm_factor_increase_field)) return;
            uint16_t norm_factor_increase = (uint16_t)cJSON_GetNumberValue(norm_factor_increase_field);
            cJSON* norm_factor_decrease_field = cJSON_GetObjectItem(params, "norm_factor_decrease");
            if (!cJSON_IsNumber(norm_factor_decrease_field)) return;
            uint16_t norm_factor_decrease = (uint16_t)cJSON_GetNumberValue(norm_factor_decrease_field);
            bitmap_generator_1bpp_mic_amplitude(t, min_norm_factor, max_norm_factor, norm_factor_increase, norm_factor_decrease);
            return;
        }
    }
}

#endif
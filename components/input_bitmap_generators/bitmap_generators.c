#include "esp_log.h"
#include <string.h>

#include "bitmap_generators.h"
#include "macros.h"
#include "util_buffer.h"
#include "util_generic.h"
#include "math.h"

// TODO: More elegant buffer type gating than just gating the entire function content


#define LOG_TAG "BITMAP-GEN"


#if defined(DISPLAY_HAS_PIXEL_BUFFER)


static uint8_t* pixel_buffer;
static size_t pixel_buffer_size = 0;
static uint16_t frame_width = 0;
static uint16_t frame_height = 0;
static cJSON* current_bitmap_generator = NULL;
static color_rgb_t white = {.r = 255, .g = 255, .b = 255};

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
    HARD_GRADIENT_3
};


void bitmap_generators_init(uint8_t* pixBuf, size_t pixBufSize, uint16_t frameWidth, uint16_t frameHeight) {
    ESP_LOGI(LOG_TAG, "Initializing bitmap generators");
    pixel_buffer = pixBuf;
    pixel_buffer_size = pixBufSize;
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

    return json;
}

void bitmap_generator_none(int64_t t) {
    
}

void bitmap_generator_solid_single(int64_t t, color_rgb_t color) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    uint8_t red = color.r * 255;
    uint8_t green = color.g * 255;
    uint8_t blue = color.b * 255;
    for (uint16_t i = 0; i < MAPPING_LENGTH; i++) {
        uint16_t pixBufIndex = LED_TO_BITMAP_MAPPING[i];
        
        pixel_buffer[pixBufIndex] = red;
        pixel_buffer[pixBufIndex + 1] = green;
        pixel_buffer[pixBufIndex + 2] = blue;
    }
    #endif
}

void bitmap_generator_rainbow_t(int64_t t, uint16_t speed) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    color_hsv_t calcColor_hsv;
    calcColor_hsv.h = (double)speed * (double)t / 1000000.0;
    calcColor_hsv.h = fmod(calcColor_hsv.h, 360.0);
    calcColor_hsv.s = 1.0;
    calcColor_hsv.v = 1.0;
    color_rgb_t color = hsv2rgb(calcColor_hsv);
    bitmap_generator_solid_single(t, color);
    #endif
}

void bitmap_generator_rainbow_gradient(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, uint8_t s, uint8_t v) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
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
    #endif
}

void bitmap_generator_hard_gradient(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, uint16_t numColors, color_rgb_t* colors) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    double angle_degrees = (double)angle;
    double angle_radians = angle_degrees * M_PI / 180.0;
    double sin_angle = sin(angle_radians);
    double cos_angle = cos(angle_radians);
    double normalized_scale = (double)scale / 100.0;

    // Calculate the diagonal length of the frame
    double diagonal_length = sqrt(frame_width * frame_width + frame_height * frame_height);
    
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
    #endif
}

void bitmap_generator_hard_gradient_3(int64_t t, uint16_t speed, uint16_t angle, uint16_t scale, color_rgb_t color1, color_rgb_t color2, color_rgb_t color3) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    color_rgb_t colors[3] = {color1, color2, color3};
    bitmap_generator_hard_gradient(t, speed, angle, scale, 3, colors);
    #endif
}

static color_rgb_t _color_rgb_from_json(cJSON* json, color_rgb_t fallback) {
    color_rgb_t color;
    cJSON* r_field = cJSON_GetObjectItem(json, "r");
    if (!cJSON_IsNumber(r_field)) return fallback;
    color.r = cJSON_GetNumberValue(r_field) / 255.0;
    cJSON* g_field = cJSON_GetObjectItem(json, "g");
    if (!cJSON_IsNumber(g_field)) return fallback;
    color.g = cJSON_GetNumberValue(g_field) / 255.0;
    cJSON* b_field = cJSON_GetObjectItem(json, "b");
    if (!cJSON_IsNumber(b_field)) return fallback;
    color.b = cJSON_GetNumberValue(b_field) / 255.0;
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
            color_rgb_t color = _color_rgb_from_json(color_obj, white);
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
            color_rgb_t color1 = _color_rgb_from_json(color1_obj, white);
            cJSON* color2_obj = cJSON_GetObjectItem(params, "color2");
            if (!cJSON_IsObject(color2_obj)) return;
            color_rgb_t color2 = _color_rgb_from_json(color2_obj, white);
            cJSON* color3_obj = cJSON_GetObjectItem(params, "color3");
            if (!cJSON_IsObject(color3_obj)) return;
            color_rgb_t color3 = _color_rgb_from_json(color3_obj, white);
            bitmap_generator_hard_gradient_3(t, speed, angle, scale, color1, color2, color3);
            return;
        }
    }
}

#endif
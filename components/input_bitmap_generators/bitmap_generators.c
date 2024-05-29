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
static cJSON* current_bitmap_generator = NULL;
static color_rgb_t white = {.r = 255, .g = 255, .b = 255};

// Order of generators. This is important!
// This order needs to match the order in which the generators
// are output by bitmap_generators_get_available().
enum generator_func {
    SOLID_SINGLE = 0,
    RAINBOW_T = 1
};


void bitmap_generators_init(uint8_t* pixBuf, size_t pixBufSize) {
    ESP_LOGI(LOG_TAG, "Initializing bitmap generators");
    pixel_buffer = pixBuf;
    pixel_buffer_size = pixBufSize;
}

void bitmap_generator_select(cJSON* bitmapGeneratorData) {
    current_bitmap_generator = bitmapGeneratorData;
}

cJSON* bitmap_generators_get_available() {
    cJSON *generator_entry, *params, *param;

    cJSON* json = cJSON_CreateObject();
    cJSON* generators_arr = cJSON_AddArrayToObject(json, "generators");

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
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 1000);
        cJSON_AddNumberToObject(param, "value", 1);
        cJSON_AddItemToObject(params, "speed", param);

    cJSON_AddItemToObject(generator_entry, "params", params);
    cJSON_AddItemToArray(generators_arr, generator_entry);

    return json;
}

void bitmap_generator_solid_single(int64_t t, color_rgb_t color) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    uint8_t red = color.r * 255;
    uint8_t green = color.g * 255;
    uint8_t blue = color.b * 255;
    for (uint32_t i = 0; i < pixel_buffer_size; i += 3) {
        pixel_buffer[i] = red;
        pixel_buffer[i + 1] = green;
        pixel_buffer[i + 2] = blue;
    }
    #endif
}

void bitmap_generator_rainbow_t(int64_t t, uint16_t speed) {
    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
    color_hsv_t calcColor_hsv;
    calcColor_hsv.h = (double)speed * (double)t / 1000000.0;
    calcColor_hsv.h = (uint16_t)fmod(calcColor_hsv.h, 360.0);
    calcColor_hsv.s = 1.0;
    calcColor_hsv.v = 1.0;
    color_rgb_t color = hsv2rgb(calcColor_hsv);
    bitmap_generator_solid_single(t, color);
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
    }
}

#endif
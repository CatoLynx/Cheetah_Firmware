/*
 * Functions for character display color effects
 */

#include "shaders_char.h"
#include "macros.h"
#include "util_generic.h"
#include "cJSON.h"
#include "esp_log.h"
#include "math.h"


#if defined(DISPLAY_HAS_TEXT_BUFFER)

#define LOG_TAG "SHD-CHAR-COLOR"

// Order of shaders. This is important!
// This order needs to match the order in which the shaders
// are output by shader_get_available().
enum shader_func {
    STATIC = 0,
    STATIC_RAINBOW = 1,
    SWEEPING_RAINBOW = 2,
    SWEEPING_SINGLE_COLOR_RAINBOW = 3,
    LINEAR_GRADIENT = 4,
};


cJSON* shader_get_available() {
    cJSON *shader_entry, *params, *param;

    cJSON* json = cJSON_CreateObject();
    cJSON* shaders_arr = cJSON_AddArrayToObject(json, "shaders");

    // Shader: Static
    shader_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(shader_entry, "name", "static");
    params = cJSON_CreateObject();

        // Parameter: Color
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "color", param);

    cJSON_AddItemToObject(shader_entry, "params", params);
    cJSON_AddItemToArray(shaders_arr, shader_entry);

    // Shader: Static Rainbow
    shader_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(shader_entry, "name", "static_rainbow");
    params = cJSON_CreateObject();

        // Parameter: Repeats
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "number");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 1);
        cJSON_AddItemToObject(params, "repeats", param);
    
    cJSON_AddItemToObject(shader_entry, "params", params);
    cJSON_AddItemToArray(shaders_arr, shader_entry);

    // Shader: Sweeping Rainbow
    shader_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(shader_entry, "name", "sweeping_rainbow");
    params = cJSON_CreateObject();

        // Parameter: Speed
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 1000);
        cJSON_AddNumberToObject(param, "value", 1);
        cJSON_AddItemToObject(params, "speed", param);

        // Parameter: Repeats
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "number");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 1);
        cJSON_AddItemToObject(params, "repeats", param);

        // Parameter: Right-to-Left
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "checkbox");
        cJSON_AddBoolToObject(param, "checked", 0);
        cJSON_AddItemToObject(params, "right_to_left", param);

    cJSON_AddItemToObject(shader_entry, "params", params);
    cJSON_AddItemToArray(shaders_arr, shader_entry);

    // Shader: Sweeping Single Color Rainbow
    shader_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(shader_entry, "name", "sweeping_single_color_rainbow");
    params = cJSON_CreateObject();

        // Parameter: Speed
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "range");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 1000);
        cJSON_AddNumberToObject(param, "value", 1);
        cJSON_AddItemToObject(params, "speed", param);

    cJSON_AddItemToObject(shader_entry, "params", params);
    cJSON_AddItemToArray(shaders_arr, shader_entry);

    // Shader: Linear Gradient
    shader_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(shader_entry, "name", "linear_gradient");
    params = cJSON_CreateObject();

        // Parameter: Start
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "start", param);

        // Parameter: End
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "color");
        cJSON_AddItemToObject(params, "end", param);

        // Parameter: Repeats
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "number");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 1);
        cJSON_AddItemToObject(params, "repeats", param);

    cJSON_AddItemToObject(shader_entry, "params", params);
    cJSON_AddItemToArray(shaders_arr, shader_entry);

    return json;
}

color_rgb_t shader_static(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character, color_rgb_t color) {
    return color;
}

color_rgb_t shader_static_rainbow(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character, uint8_t repeats) {
    color_hsv_t calcColor_hsv;
    uint16_t span = DISPLAY_VIEWPORT_WIDTH_CHAR / repeats;
    if (span == 0) span = 1;
    calcColor_hsv.h = (cb_i_display % span) * (360 / span);
    calcColor_hsv.s = 1.0;
    calcColor_hsv.v = 1.0;
    return hsv2rgb(calcColor_hsv);
}

color_rgb_t shader_sweeping_rainbow(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character, uint16_t speed, uint8_t repeats, uint8_t rtl) {
    color_hsv_t calcColor_hsv;
    uint16_t span = DISPLAY_VIEWPORT_WIDTH_CHAR / repeats;
    if (span == 0) span = 1;
    if (rtl) {
        calcColor_hsv.h = (cb_i_display % span) * (360 / span);
    } else {
        calcColor_hsv.h = ((charBufSize - cb_i_display) % span) * (360 / span);
    }
    calcColor_hsv.h += (double)speed * (double)time_getSystemTime_us() / 1000000.0;
    calcColor_hsv.h = (uint16_t)fmod(calcColor_hsv.h, 360.0);
    calcColor_hsv.s = 1.0;
    calcColor_hsv.v = 1.0;
    return hsv2rgb(calcColor_hsv);
}

color_rgb_t shader_sweeping_single_color_rainbow(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character, uint16_t speed) {
    color_hsv_t calcColor_hsv;
    calcColor_hsv.h = (double)speed * (double)time_getSystemTime_us() / 1000000.0;
    calcColor_hsv.h = (uint16_t)fmod(calcColor_hsv.h, 360.0);
    calcColor_hsv.s = 1.0;
    calcColor_hsv.v = 1.0;
    return hsv2rgb(calcColor_hsv);
}

color_rgb_t shader_linear_gradient(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character, color_rgb_t start, color_rgb_t end, uint8_t repeats) {
    color_rgb_t calcColor;
    uint16_t span = DISPLAY_VIEWPORT_WIDTH_CHAR / repeats;
    if (span == 0) span = 1;
    calcColor.r = map_double(cb_i_display % span, 0, span - 1, start.r, end.r);
    calcColor.g = map_double(cb_i_display % span, 0, span - 1, start.g, end.g);
    calcColor.b = map_double(cb_i_display % span, 0, span - 1, start.b, end.b);
    return calcColor;
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

color_rgb_t shader_fromJSON(uint16_t cb_i_display, uint16_t charBufSize, uint8_t character, cJSON* shaderData) {
    // Fall back to black/off in case of error
    color_rgb_t fallback = { .r = 1.0, .g = 1.0, .b = 1.0 };

    if (shaderData == NULL) return fallback;

    cJSON* shader_id_field = cJSON_GetObjectItem(shaderData, "shader");
    if (!cJSON_IsNumber(shader_id_field)) return fallback;
    enum shader_func shaderId = (enum shader_func)cJSON_GetNumberValue(shader_id_field);

    cJSON* params = cJSON_GetObjectItem(shaderData, "params");
    if (!cJSON_IsObject(params)) return fallback;

    switch (shaderId) {
        case STATIC: {
            cJSON* color_obj = cJSON_GetObjectItem(params, "color");
            if (!cJSON_IsObject(color_obj)) return fallback;
            color_rgb_t color = _color_rgb_from_json(color_obj, fallback);
            ESP_LOGV(LOG_TAG, "shader=%p shaderId=%u color=%.2f, %.2f, %.2f", shaderData, shaderId, color.r, color.g, color.b);
            return shader_static(cb_i_display, charBufSize, character, color);
        }
        
        case STATIC_RAINBOW: {
            cJSON* repeats_field = cJSON_GetObjectItem(params, "repeats");
            if (!cJSON_IsNumber(repeats_field)) return fallback;
            uint8_t repeats = (uint8_t)cJSON_GetNumberValue(repeats_field);
            // Prevent 0
            repeats = repeats ? repeats : 1;

            color_rgb_t color = shader_static_rainbow(cb_i_display, charBufSize, character, repeats);
            ESP_LOGV(LOG_TAG, "shader=%p shaderId=%u color=%.2f, %.2f, %.2f", shaderData, shaderId, color.r, color.g, color.b);
            return color;
        }
        
        case SWEEPING_RAINBOW: {
            cJSON* speed_field = cJSON_GetObjectItem(params, "speed");
            if (!cJSON_IsNumber(speed_field)) return fallback;
            uint16_t speed = (uint16_t)cJSON_GetNumberValue(speed_field);
            
            cJSON* repeats_field = cJSON_GetObjectItem(params, "repeats");
            if (!cJSON_IsNumber(repeats_field)) return fallback;
            uint8_t repeats = (uint8_t)cJSON_GetNumberValue(repeats_field);
            // Prevent 0
            repeats = repeats ? repeats : 1;
            
            cJSON* rtl_field = cJSON_GetObjectItem(params, "right_to_left");
            if (!cJSON_IsBool(rtl_field)) return fallback;
            uint8_t rtl = (uint8_t)cJSON_IsTrue(rtl_field);

            color_rgb_t color = shader_sweeping_rainbow(cb_i_display, charBufSize, character, speed, repeats, rtl);
            ESP_LOGV(LOG_TAG, "shader=%p shaderId=%u color=%.2f, %.2f, %.2f", shaderData, shaderId, color.r, color.g, color.b);
            return color;
        }
        
        case SWEEPING_SINGLE_COLOR_RAINBOW: {
            cJSON* speed_field = cJSON_GetObjectItem(params, "speed");
            if (!cJSON_IsNumber(speed_field)) return fallback;
            uint16_t speed = (uint16_t)cJSON_GetNumberValue(speed_field);

            color_rgb_t color = shader_sweeping_single_color_rainbow(cb_i_display, charBufSize, character, speed);
            ESP_LOGV(LOG_TAG, "shader=%p shaderId=%u color=%.2f, %.2f, %.2f", shaderData, shaderId, color.r, color.g, color.b);
            return color;
        }
        
        case LINEAR_GRADIENT: {
            cJSON* start_obj = cJSON_GetObjectItem(params, "start");
            if (!cJSON_IsObject(start_obj)) return fallback;
            color_rgb_t start = _color_rgb_from_json(start_obj, fallback);
            
            cJSON* end_obj = cJSON_GetObjectItem(params, "end");
            if (!cJSON_IsObject(end_obj)) return fallback;
            color_rgb_t end = _color_rgb_from_json(end_obj, fallback);

            cJSON* repeats_field = cJSON_GetObjectItem(params, "repeats");
            if (!cJSON_IsNumber(repeats_field)) return fallback;
            uint8_t repeats = (uint8_t)cJSON_GetNumberValue(repeats_field);
            // Prevent 0
            repeats = repeats ? repeats : 1;

            color_rgb_t color = shader_linear_gradient(cb_i_display, charBufSize, character, start, end, repeats);
            ESP_LOGV(LOG_TAG, "shader=%p shaderId=%u color=%.2f, %.2f, %.2f", shaderData, shaderId, color.r, color.g, color.b);
            return color;
        }
    }

    return fallback;
}

#endif
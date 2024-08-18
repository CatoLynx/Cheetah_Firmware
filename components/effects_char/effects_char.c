/*
 * Functions for character display effects
 */

#include "effects_char.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "util_generic.h"
#include "macros.h"


#if defined(DISPLAY_HAS_TEXT_BUFFER)

#define LOG_TAG "EFF-CHAR"

// Order of effects. This is important!
// This order needs to match the order in which the effects
// are output by effect_get_available().
enum effect_func {
    NONE = 0,
    GLITCHES = 1
};


cJSON* effect_get_available() {
    cJSON *effect_entry, *params, *param;

    cJSON* json = cJSON_CreateObject();
    cJSON* effects_arr = cJSON_AddArrayToObject(json, "effects");

    // Effect: None
    effect_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(effect_entry, "name", "none");
    params = cJSON_CreateObject();
    cJSON_AddItemToObject(effect_entry, "params", params);
    cJSON_AddItemToArray(effects_arr, effect_entry);

    // Effect: Glitches
    effect_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(effect_entry, "name", "glitches");
    params = cJSON_CreateObject();

        // Parameter: Character Percentage
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "number");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 1);
        cJSON_AddItemToObject(params, "character_percentage", param);

        // Parameter: Average glitch duration
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "number");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 1000);
        cJSON_AddNumberToObject(param, "value", 50);
        cJSON_AddItemToObject(params, "duration_avg_ms", param);

        // Parameter: Glitch duration spread
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "number");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 1000);
        cJSON_AddNumberToObject(param, "value", 50);
        cJSON_AddItemToObject(params, "duration_spread_ms", param);

        // Parameter: Average glitch interval
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "number");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 10000);
        cJSON_AddNumberToObject(param, "value", 1000);
        cJSON_AddItemToObject(params, "interval_avg_ms", param);

        // Parameter: Glitch interval spread
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "number");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 10000);
        cJSON_AddNumberToObject(param, "value", 1000);
        cJSON_AddItemToObject(params, "interval_spread_ms", param);
    
    cJSON_AddItemToObject(effect_entry, "params", params);
    cJSON_AddItemToArray(effects_arr, effect_entry);

    return json;
}

// Return value of effect functions is 1 if the char buffer was modified, 0 otherwise

typedef struct {
    uint8_t active;
    int64_t start_time_us;
    int64_t duration_us;
    uint8_t charVal;
} glitch_mod_t;
static int64_t effect_glitches_last_run_us = 0;
static glitch_mod_t* effect_glitches_mods = NULL;
uint8_t effect_glitches(uint8_t* charBuf, size_t charBufSize, uint8_t character_percentage, uint16_t duration_avg_ms, uint16_t duration_spread_ms, uint16_t interval_avg_ms, uint16_t interval_spread_ms) {
    uint8_t modified = 0;
    // On first call, init buffer for modifications
    // This will never get freed
    if (effect_glitches_mods == NULL) {
        effect_glitches_mods = malloc(sizeof(glitch_mod_t) * charBufSize);
    }

    int64_t now_us = esp_timer_get_time();
    //int64_t delta_us = now_us - effect_glitches_last_run_us;
    effect_glitches_last_run_us = now_us;

    for (uint16_t i = 0; i < charBufSize; i++) {
        // Check if any active mods have expired
        if (effect_glitches_mods[i].active) {
            if (now_us >= effect_glitches_mods[i].start_time_us + effect_glitches_mods[i].duration_us) {
                // Expired
                effect_glitches_mods[i].active = 0;
            } else {
                // Ongoing
                charBuf[i] = effect_glitches_mods[i].charVal;
                modified = 1;
            }
        } else {
            // Inactive, modify if probability allows it
            if (rand_range(0, 100) < character_percentage) {
                effect_glitches_mods[i].active = 1;
                effect_glitches_mods[i].start_time_us = now_us;
                effect_glitches_mods[i].duration_us = 1000 * rand_spread(duration_avg_ms, duration_spread_ms);
                effect_glitches_mods[i].charVal = rand_range(32, 128);
                charBuf[i] = effect_glitches_mods[i].charVal;
                modified = 1;
            }
        }
    }

    return modified;
}

uint8_t effect_fromJSON(uint8_t* charBuf, size_t charBufSize, cJSON* effectData) {
    if (effectData == NULL) return 0;

    cJSON* effect_id_field = cJSON_GetObjectItem(effectData, "effect");
    if (!cJSON_IsNumber(effect_id_field)) return 0;
    enum effect_func effectId = (enum effect_func)cJSON_GetNumberValue(effect_id_field);

    cJSON* params = cJSON_GetObjectItem(effectData, "params");
    if (!cJSON_IsObject(params)) return 0;

    switch (effectId) {
        case NONE: {
            return 0;
        }
        
        case GLITCHES: {
            cJSON* character_percentage_field = cJSON_GetObjectItem(params, "character_percentage");
            if (!cJSON_IsNumber(character_percentage_field)) return 0;
            uint8_t character_percentage = (uint8_t)cJSON_GetNumberValue(character_percentage_field);

            cJSON* duration_avg_ms_field = cJSON_GetObjectItem(params, "duration_avg_ms");
            if (!cJSON_IsNumber(duration_avg_ms_field)) return 0;
            uint16_t duration_avg_ms = (uint16_t)cJSON_GetNumberValue(duration_avg_ms_field);

            cJSON* duration_spread_ms_field = cJSON_GetObjectItem(params, "duration_spread_ms");
            if (!cJSON_IsNumber(duration_spread_ms_field)) return 0;
            uint16_t duration_spread_ms = (uint16_t)cJSON_GetNumberValue(duration_spread_ms_field);

            cJSON* interval_avg_ms_field = cJSON_GetObjectItem(params, "interval_avg_ms");
            if (!cJSON_IsNumber(interval_avg_ms_field)) return 0;
            uint16_t interval_avg_ms = (uint16_t)cJSON_GetNumberValue(interval_avg_ms_field);

            cJSON* interval_spread_ms_field = cJSON_GetObjectItem(params, "interval_spread_ms");
            if (!cJSON_IsNumber(interval_spread_ms_field)) return 0;
            uint16_t interval_spread_ms = (uint16_t)cJSON_GetNumberValue(interval_spread_ms_field);

            ESP_LOGV(LOG_TAG, "effect=%p effectId=%u", effectData, effectId);
            return effect_glitches(charBuf, charBufSize, character_percentage, duration_avg_ms, duration_spread_ms, interval_avg_ms, interval_spread_ms);
        }
    }

    return 0;
}

#endif
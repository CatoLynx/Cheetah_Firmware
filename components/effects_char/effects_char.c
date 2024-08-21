/*
 * Functions for character display effects
 */

#include "effects_char.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "util_generic.h"
#include "macros.h"
#include <string.h>


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

        // Parameter: Probability
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "number");
        cJSON_AddNumberToObject(param, "min", 0);
        cJSON_AddNumberToObject(param, "max", 10000);
        cJSON_AddNumberToObject(param, "value", 1);
        cJSON_AddItemToObject(params, "probability", param);

        // Parameter: Average glitch duration
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "number");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 10000);
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

        // Parameter: Non-blank only
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "checkbox");
        cJSON_AddBoolToObject(param, "checked", 0);
        cJSON_AddItemToObject(params, "non_blank_only", param);
    
    cJSON_AddItemToObject(effect_entry, "params", params);
    cJSON_AddItemToArray(effects_arr, effect_entry);

    return json;
}

// Return value of effect functions is 1 if the char buffer was modified, 0 otherwise

typedef struct {
    uint8_t active;
    int64_t next_processing_time_us;
    int64_t start_time_us;
    int64_t duration_us;
    uint8_t charVal;
} glitch_mod_t;
static int64_t effect_glitches_last_run_us = 0;
static glitch_mod_t* effect_glitches_mods = NULL;
uint8_t effect_glitches(uint8_t* charBuf, size_t charBufSize, uint16_t probability, uint16_t duration_avg_ms, uint16_t duration_spread_ms, uint16_t interval_avg_ms, uint16_t interval_spread_ms, uint8_t non_blank_only) {
    uint8_t modified = 0;
    // On first call, init buffer for modifications
    // This will never get freed
    if (effect_glitches_mods == NULL) {
        effect_glitches_mods = malloc(sizeof(glitch_mod_t) * charBufSize);
        memset(effect_glitches_mods, 0x00, sizeof(glitch_mod_t) * charBufSize);
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
        }

        // Check if any mods can be enabled
        // This is not an if-else since the same mod couldn't be
        // instantly re-enabled in that case
        if (!effect_glitches_mods[i].active) {
            // Check if non-blank, if applicable
            if (!non_blank_only || (non_blank_only && charBuf[i] != ' ')) {
                // Check if mod is ready to be processed again
                if (now_us >= effect_glitches_mods[i].next_processing_time_us) {
                    // Update next processing time
                    effect_glitches_mods[i].next_processing_time_us = now_us + 1000 * rand_spread(interval_avg_ms, interval_spread_ms);

                    // Inactive, modify if probability allows it
                    if (rand_range(0, 10000) < probability) {
                        effect_glitches_mods[i].active = 1;
                        effect_glitches_mods[i].start_time_us = now_us;
                        effect_glitches_mods[i].duration_us = 1000 * rand_spread(duration_avg_ms, duration_spread_ms);
                        effect_glitches_mods[i].charVal = rand_range(32, 128);
                        charBuf[i] = effect_glitches_mods[i].charVal;
                        modified = 1;
                    }
                }
            }
        }
    }

    return modified;
}

uint8_t effect_fromJSON(uint8_t* charBuf, size_t charBufSize, cJSON* effectData) {
    // TODO: Returning 1 instead of 0 is only a workaround. This whole thing is not quite right
    if (effectData == NULL) return 1;

    cJSON* effect_id_field = cJSON_GetObjectItem(effectData, "effect");
    if (!cJSON_IsNumber(effect_id_field)) return 1;
    enum effect_func effectId = (enum effect_func)cJSON_GetNumberValue(effect_id_field);

    cJSON* params = cJSON_GetObjectItem(effectData, "params");
    if (!cJSON_IsObject(params)) return 1;

    switch (effectId) {
        case NONE: {
            return 1;
        }
        
        case GLITCHES: {
            cJSON* probability_field = cJSON_GetObjectItem(params, "probability");
            if (!cJSON_IsNumber(probability_field)) return 1;
            uint16_t probability = (uint16_t)cJSON_GetNumberValue(probability_field);

            cJSON* duration_avg_ms_field = cJSON_GetObjectItem(params, "duration_avg_ms");
            if (!cJSON_IsNumber(duration_avg_ms_field)) return 1;
            uint16_t duration_avg_ms = (uint16_t)cJSON_GetNumberValue(duration_avg_ms_field);

            cJSON* duration_spread_ms_field = cJSON_GetObjectItem(params, "duration_spread_ms");
            if (!cJSON_IsNumber(duration_spread_ms_field)) return 1;
            uint16_t duration_spread_ms = (uint16_t)cJSON_GetNumberValue(duration_spread_ms_field);

            cJSON* interval_avg_ms_field = cJSON_GetObjectItem(params, "interval_avg_ms");
            if (!cJSON_IsNumber(interval_avg_ms_field)) return 1;
            uint16_t interval_avg_ms = (uint16_t)cJSON_GetNumberValue(interval_avg_ms_field);

            cJSON* interval_spread_ms_field = cJSON_GetObjectItem(params, "interval_spread_ms");
            if (!cJSON_IsNumber(interval_spread_ms_field)) return 1;
            uint16_t interval_spread_ms = (uint16_t)cJSON_GetNumberValue(interval_spread_ms_field);
            
            cJSON* non_blank_only_field = cJSON_GetObjectItem(params, "non_blank_only");
            if (!cJSON_IsBool(non_blank_only_field)) return 1;
            uint8_t non_blank_only = (uint8_t)cJSON_IsTrue(non_blank_only_field);

            ESP_LOGV(LOG_TAG, "effect=%p effectId=%u", effectData, effectId);
            return effect_glitches(charBuf, charBufSize, probability, duration_avg_ms, duration_spread_ms, interval_avg_ms, interval_spread_ms, non_blank_only);
        }
    }

    return 1;
}

#endif
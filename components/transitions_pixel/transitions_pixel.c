/*
 * Functions for pixel buffer transition effects
 */

#include "transitions_pixel.h"
#include "macros.h"
#include "util_generic.h"
#include "esp_log.h"
#include "math.h"
#include <string.h>


#define LOG_TAG "TRANS-PIX"


// Order of transitions. This is important!
// This order needs to match the order in which the transitions
// are output by transition_get_available().
enum transition_func {
    INSTANT = 0,
    WIPE_LTR = 1
};


cJSON* transition_get_available() {
    cJSON *transition_entry, *params, *param;

    cJSON* json = cJSON_CreateObject();
    cJSON* transitions_arr = cJSON_AddArrayToObject(json, "transitions");

    // Transition: Instant
    transition_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(transition_entry, "name", "instant");
    params = cJSON_CreateObject();

    cJSON_AddObjectToObject(transition_entry, "params");
    cJSON_AddItemToArray(transitions_arr, transition_entry);

    // Transition: Wipe left to right
    transition_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(transition_entry, "name", "wipe_ltr");
    params = cJSON_CreateObject();

        // Parameter: Speed
        param = cJSON_CreateObject();
        cJSON_AddStringToObject(param, "type", "number");
        cJSON_AddNumberToObject(param, "min", 1);
        cJSON_AddNumberToObject(param, "max", 100);
        cJSON_AddNumberToObject(param, "value", 1);
        cJSON_AddItemToObject(params, "speed", param);
    
    cJSON_AddItemToObject(transition_entry, "params", params);
    cJSON_AddItemToArray(transitions_arr, transition_entry);

    return json;
}

void transition_instant(uint8_t* oldPixBuf, uint8_t* newPixBuf, uint8_t* outPixBuf, size_t pixBufSize) {
    memcpy(outPixBuf, newPixBuf, pixBufSize);
}

void transition_fromJSON(uint8_t* oldPixBuf, uint8_t* newPixBuf, uint8_t* outPixBuf, size_t pixBufSize, cJSON* transitionData) {
    if (transitionData == NULL) return;

    cJSON* transition_id_field = cJSON_GetObjectItem(transitionData, "transition");
    if (!cJSON_IsNumber(transition_id_field)) return;
    enum transition_func transitionId = (enum transition_func)cJSON_GetNumberValue(transition_id_field);
    ESP_LOGV(LOG_TAG, "transition=%p transitionId=%u", transitionData, transitionId);

    cJSON* params = cJSON_GetObjectItem(transitionData, "params");
    if (!cJSON_IsObject(params)) return;

    switch (transitionId) {
        case INSTANT: {
            transition_instant(oldPixBuf, newPixBuf, outPixBuf, pixBufSize);
            return;
        }
        
        case WIPE_LTR: {
            return;
            /*cJSON* speed_field = cJSON_GetObjectItem(params, "speed");
            if (!cJSON_IsNumber(speed_field)) return;
            uint8_t speed = (uint8_t)cJSON_GetNumberValue(speed_field);
            // Prevent 0
            speed = speed ? speed : 1;

            transition_wipe_ltr(oldPixBuf, newPixBuf, outPixBuf, pixBufSize, speed);
            return color;*/
        }
    }

    return;
}
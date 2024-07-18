#include "util_disp_selection.h"

#include "esp_log.h"
#include "util_nvs.h"
#include "macros.h"
#include <stdio.h>


// Load the configuration for a selection-based display from the JSON file in SPIFFS
// whose name is stored in NVS and store the cJSON object in the given pointer
esp_err_t display_selection_loadConfiguration(nvs_handle_t* nvsHandle, cJSON** json, const char* log_tag) {
    char* confFile = get_string_from_nvs(nvsHandle, "sel_conf_file");
    if (confFile == NULL) {
        ESP_LOGE(log_tag, "Failed to get configuration file name from NVS");
        return ESP_FAIL;
    }
    esp_err_t ret = get_json_from_spiffs(confFile, json, log_tag);
    free(confFile);
    return ret;
}

// Load the configuration for a selection-based display from the JSON file in SPIFFS
// whose name is stored in NVS and set the framebuffer mask and unit count based on the data therein.
esp_err_t display_selection_loadAndParseConfiguration(nvs_handle_t* nvsHandle, uint8_t* display_framebuf_mask, uint16_t* display_num_units, const char* log_tag) {
    cJSON* json;
    esp_err_t ret = display_selection_loadConfiguration(nvsHandle, &json, log_tag);
    if (ret != ESP_OK) return ret;

    if (!cJSON_IsObject(json)) {
        ESP_LOGE(log_tag, "No valid JSON object found");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    cJSON* units_arr = cJSON_GetObjectItem(json, "units");
    if (!cJSON_IsArray(units_arr)) {
        ESP_LOGE(log_tag, "Key 'units' not found in JSON or is not an array");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    cJSON* entry = NULL;
    cJSON_ArrayForEach(entry, units_arr) {
        if (!cJSON_IsObject(entry)) {
            ESP_LOGE(log_tag, "Item in 'units' array is not an object");
            cJSON_Delete(json);
            return ESP_FAIL;
        }

        cJSON* field_addr = cJSON_GetObjectItem(entry, "addr");
        if (!cJSON_IsNumber(field_addr)) {
            ESP_LOGE(log_tag, "Key 'addr' of unit entry is not a number");
            cJSON_Delete(json);
            return ESP_FAIL;
        }
        uint16_t unitAddr = cJSON_GetNumberValue(field_addr);

        cJSON* field_len = cJSON_GetObjectItem(entry, "len");
        if (!cJSON_IsNumber(field_len)) {
            ESP_LOGE(log_tag, "Key 'len' of unit entry is not a number");
            cJSON_Delete(json);
            return ESP_FAIL;
        }
        uint16_t unitLen = cJSON_GetNumberValue(field_len);

        for (uint16_t i = 0; i < unitLen; i++) {
            // Mark each unit as present in the framebuf mask
            SET_MASK(display_framebuf_mask, unitAddr + i);
            (*display_num_units)++;
        }
    }

    ESP_LOGI(log_tag, "Number of units: %d", *display_num_units);

    cJSON_Delete(json);
    return ESP_OK;
}
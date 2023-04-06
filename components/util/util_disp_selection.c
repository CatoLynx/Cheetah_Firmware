#include "util_disp_selection.h"

#include "esp_log.h"
#include "util_nvs.h"
#include "macros.h"
#include <stdio.h>
#include "cJSON.h"

// Load the configuration for a selection-based display from the JSON file in SPIFFS
// whose name is stored in NVS and set the framebuffer mask and unit count based on the data therein.
esp_err_t display_selection_loadConfiguration(nvs_handle_t* nvsHandle, uint8_t* display_framebuf_mask, uint16_t* display_num_units, const char* log_tag) {
    char* confFile = get_string_from_nvs(nvsHandle, "sel_conf_file");
    if (confFile == NULL) {
        ESP_LOGE(log_tag, "Failed to get configuration file name from NVS");
        return ESP_FAIL;
    }

    char file_path[21]; // "/spiffs/" + 8.3 filename + null
    snprintf(file_path, 21, "/spiffs/%s", confFile);
    free(confFile);
    
    ESP_LOGI(log_tag, "Opening file: %s", file_path);
    FILE* file = fopen(file_path, "rb");
    if (file == NULL) {
        ESP_LOGE(log_tag, "Failed to open file");
        return ESP_FAIL;
    }
    // Seek to end to get length
    if (fseek(file, 0, SEEK_END) != 0) {
        ESP_LOGE(log_tag, "fseek failed");
        return ESP_FAIL;
    }
    long fsize = ftell(file);
    if (fsize == -1) {
        ESP_LOGE(log_tag, "ftell failed");
        return ESP_FAIL;
    }
    // Seek back to beginning
    if (fseek(file, 0, SEEK_SET) != 0) {
        ESP_LOGE(log_tag, "fseek failed");
        return ESP_FAIL;
    }

    // Allocate buffer for file contents (+1 for null terminator)
    char* raw_json = malloc(fsize + 1);
    size_t read = fread(raw_json, 1, (size_t)fsize, file);
    if (read != fsize) {
        ESP_LOGE(log_tag, "read size doesn't equal file size");
        free(raw_json);
        return ESP_FAIL;
    }
    cJSON* json = cJSON_Parse(raw_json);
    free(raw_json);

    fclose(file);

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
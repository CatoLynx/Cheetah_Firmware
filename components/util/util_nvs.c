#include "util_nvs.h"
#include "esp_log.h"


char* get_string_from_nvs(nvs_handle_t* nvsHandle, const char* key) {
    // Query string length
    size_t valueLength;
    char* value;
    esp_err_t ret = nvs_get_str(*nvsHandle, key, NULL, &valueLength);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return NULL;
    } else {
        ESP_ERROR_CHECK(ret);
        value = malloc(valueLength);
        // Read value
        ESP_ERROR_CHECK(nvs_get_str(*nvsHandle, key, value, &valueLength));
    }
    return value;
}

// Load a JSON file from SPIFFS and store the cJSON object in the given pointer
esp_err_t get_json_from_spiffs(const char* spiffsFileName, cJSON** json, const char* log_tag) {
    char file_path[21]; // "/spiffs/" + 8.3 filename + null
    snprintf(file_path, 21, "/spiffs/%s", spiffsFileName);
    
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
    *json = cJSON_Parse(raw_json);
    free(raw_json);
    fclose(file);
    return ESP_OK;
}
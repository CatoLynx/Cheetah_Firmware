#include "util_nvs.h"


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
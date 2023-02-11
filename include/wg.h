#include "esp_wireguard.h"
#include "util_nvs.h"

esp_err_t wg_init(nvs_handle_t* nvsHandle);
esp_err_t wg_start();
bool wg_is_up();
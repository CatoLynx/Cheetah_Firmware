#include "macros.h"
#include "esp_netif.h"
#include "esp_wireguard.h"
#include "util_nvs.h"

esp_err_t wg_init(nvs_handle_t* nvsHandle);
esp_err_t wg_start(esp_netif_t* base_if);
esp_err_t wg_stop(void);
esp_err_t wg_start_wifi(void);
esp_err_t wg_start_eth(void);
bool wg_is_up();
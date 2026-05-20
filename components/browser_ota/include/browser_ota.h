#pragma once

#include "esp_http_server.h"
#include "nvs.h"

void browser_ota_init(httpd_handle_t* server, nvs_handle_t* nvsHandle);
void browser_ota_deinit(void);
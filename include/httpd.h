#pragma once

#include "esp_http_server.h"
#include "nvs_flash.h"

httpd_handle_t httpd_init(nvs_handle_t* nvsHandle);
void httpd_deinit(httpd_handle_t server);
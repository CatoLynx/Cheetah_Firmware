#pragma once

#include "esp_http_server.h"

void browser_ota_init(httpd_handle_t* server);
void browser_ota_deinit(void);
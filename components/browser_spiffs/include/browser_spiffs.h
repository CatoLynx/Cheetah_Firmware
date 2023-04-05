#pragma once

#include "esp_http_server.h"


void browser_spiffs_init(httpd_handle_t* server);
void browser_spiffs_deinit(void);
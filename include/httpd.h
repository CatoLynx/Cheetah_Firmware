#pragma once

#include "esp_http_server.h"

httpd_handle_t httpd_init(void);
void httpd_deinit(httpd_handle_t server);
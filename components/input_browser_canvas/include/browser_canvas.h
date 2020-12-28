#pragma once

#include "esp_http_server.h"

void browser_canvas_init(httpd_handle_t* server, uint8_t* outBuf, size_t bufSize);
void browser_canvas_deinit(void);
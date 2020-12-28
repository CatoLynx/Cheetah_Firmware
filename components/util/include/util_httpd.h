#pragma once

#include "esp_http_server.h"

esp_err_t abortRequest(httpd_req_t *req, const char* message);
esp_err_t post_recv_handler(const char* log_tag, httpd_req_t *req, uint8_t* dest, uint32_t max_size);
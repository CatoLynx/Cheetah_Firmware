#pragma once

#include "esp_http_server.h"


#define HTTPD_401 "401 Unauthorized"


typedef struct {
    char* username;
    char* password;
    char* realm;
} basic_auth_info_t;


esp_err_t abortRequest(httpd_req_t *req, const char* message);
esp_err_t post_recv_handler(const char* log_tag, httpd_req_t *req, uint8_t* dest, uint32_t max_size);
char* http_auth_basic_digest(const char *username, const char *password);
bool basic_auth_handler(httpd_req_t* req, const char* log_tag);
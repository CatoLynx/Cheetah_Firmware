#pragma once

#include "esp_http_server.h"
#include "nvs.h"


typedef enum config_data_type {
    I8   = 10,
    I16  = 11,
    I32  = 12,
    I64  = 13,
    U8   = 14,
    U16  = 15,
    U32  = 16,
    U64  = 17,
    STR  = 18,
    BLOB = 19
} config_data_type_t;

typedef struct config_entry {
    char* key;
    config_data_type_t dataType;
} config_entry_t;


void browser_config_init(httpd_handle_t* server, nvs_handle_t* nvsHandle);
void browser_config_deinit(void);
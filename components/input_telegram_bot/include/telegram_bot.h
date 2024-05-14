#pragma once

#include "macros.h"
#include "esp_http_client.h"
#include "nvs.h"
#include "cJSON.h"


typedef enum {
    TG_GET_ME,
    TG_GET_UPDATES,
    TG_SEND_MESSAGE,
} telegram_api_endpoint_t;

esp_err_t telegram_bot_http_event_handler(esp_http_client_event_t *evt);
void telegram_bot_init(nvs_handle_t* nvsHandle, uint8_t* outBuf, size_t bufSize);
void telegram_bot_deinit();
void telegram_bot_task(void* arg);
void telegram_bot_send_request(telegram_api_endpoint_t endpoint, ...);
esp_err_t telegram_bot_process_response(telegram_api_endpoint_t endpoint, cJSON* json);
#pragma once

#include "esp_http_client.h"
#include "nvs.h"
#include "cJSON.h"


typedef struct {
    uint8_t* buffer;
    uint16_t duration;
} rp_buffer_list_entry_t;


esp_err_t remote_poll_http_event_handler(esp_http_client_event_t *evt);
void remote_poll_init(nvs_handle_t* nvsHandle, uint8_t* outBuf, size_t bufSize);
void remote_poll_deinit();
void remote_poll_task(void* arg);
void remote_poll_send_request();
esp_err_t remote_poll_process_response(cJSON* json);
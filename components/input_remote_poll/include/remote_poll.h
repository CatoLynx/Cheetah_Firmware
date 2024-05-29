#pragma once

#include "esp_http_client.h"
#include "nvs.h"
#include "cJSON.h"


#define MAX_NUM_BUFFERS 2048


typedef struct {
    uint8_t* pixelBuffer;
    uint8_t* textBuffer;
    uint8_t* unitBuffer;
    uint16_t duration;
} rp_buffer_list_entry_t;


esp_err_t remote_poll_http_event_handler(esp_http_client_event_t *evt);
void remote_poll_init(nvs_handle_t* nvsHandle, uint8_t* pixBuf, size_t pixBufSize, uint8_t* textBuf, size_t textBufSize, uint8_t* unitBuf, size_t unitBufSize);
void remote_poll_deinit();
void remote_poll_task(void* arg);
void remote_poll_send_request();
esp_err_t remote_poll_process_response(cJSON* json);
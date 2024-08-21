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
} pl_buffer_list_entry_t;


esp_err_t playlist_http_event_handler(esp_http_client_event_t *evt);
void playlist_init(nvs_handle_t* nvsHandle, uint8_t* pixBuf, size_t pixBufSize, uint8_t* textBuf, size_t textBufSize, uint8_t* unitBuf, size_t unitBufSize);
void playlist_deinit();
void playlist_task(void* arg);
void playlist_update_from_http();
void playlist_update_from_file();
esp_err_t playlist_process_json(cJSON* json);
#pragma once

#include "esp_http_client.h"
#include "nvs.h"
#include "cJSON.h"


typedef struct {
    uint8_t* pixelBuffer;
    uint8_t* textBuffer;
    uint8_t* unitBuffer;
    uint16_t duration;
    int16_t brightness;
    cJSON* shader;
    uint8_t updateShader;
    cJSON* transition;
    uint8_t updateTransition;
    cJSON* effect;
    uint8_t updateEffect;
    cJSON* bitmapGenerator;
    uint8_t updateBitmapGenerator;
} pl_buffer_list_entry_t;

typedef struct {
    pl_buffer_list_entry_t* entries;
    uint16_t numEntries;
} pl_buffer_group_t;

typedef enum {
    PL_SEQUENTIAL,
    PL_RANDOM
} pl_mode_t;


esp_err_t playlist_http_event_handler(esp_http_client_event_t *evt);
void playlist_init(nvs_handle_t* nvsHandle, uint8_t* pixBuf, size_t pixBufSize, uint8_t* textBuf, size_t textBufSize, uint8_t* unitBuf, size_t unitBufSize);
void playlist_deinit();
void playlist_register_brightness(uint8_t* brightness);
void playlist_register_shaders(cJSON** shaderData, uint8_t* shaderDataDeletable);
void playlist_register_transitions(cJSON** transitionData, uint8_t* transitionDataDeletable);
void playlist_register_effects(cJSON** effectData, uint8_t* effectDataDeletable);
void playlist_register_bitmap_generators(cJSON** bitmapGeneratorData, uint8_t* bitmapGeneratorDataDeletable);
void playlist_task(void* arg);
void playlist_update_from_http();
void playlist_update_from_file();
esp_err_t playlist_process_json(cJSON* json);
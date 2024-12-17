#pragma once

#include <stdint.h>
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t display_init(nvs_handle_t* nvsHandle);
void display_update(uint8_t* pixBuf, uint8_t* prevPixBuf, size_t pixBufSize, portMUX_TYPE* pixBufLock);
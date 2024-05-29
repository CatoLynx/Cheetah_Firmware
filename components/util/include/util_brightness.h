#pragma once

#include "esp_system.h"

esp_err_t brightness_sensor_init(uint8_t* brightnessValue, uint8_t* baseBrightnessValue);
void brightness_adjust_task(void* arg);
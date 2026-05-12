#pragma once

#include "esp_system.h"

esp_err_t brightness_sensor_init(uint8_t* brightnessValue, uint8_t* baseBrightnessValue);
void brightness_adjust_task(void* arg);
esp_err_t brightness_control_init(void);
void display_set_brightness(uint8_t brightness);
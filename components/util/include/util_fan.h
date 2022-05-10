#pragma once

#include "driver/ledc.h"
#include "esp_system.h"

esp_err_t fan_init();
esp_err_t fan_set_speed(uint8_t speed);
void fan_set_target_speed(uint8_t speed);
void fan_speed_adjust_task(void* arg);
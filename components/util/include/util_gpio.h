#pragma once

#include "driver/gpio.h"
#include "esp_system.h"

esp_err_t gpio_pulse(gpio_num_t gpio_num, uint8_t level, uint32_t pulseWidth, uint32_t delayAfter);
esp_err_t gpio_pulse_inv(gpio_num_t gpio_num, uint8_t level, uint32_t pulseWidth, uint32_t delayAfter, uint8_t activeLow);
esp_err_t gpio_set(gpio_num_t gpio_num, uint8_t level, uint8_t activeLow);
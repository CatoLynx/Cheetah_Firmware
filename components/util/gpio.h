/*
 * Header file for GPIO utils
 */

#ifndef MAIN_UTIL_GPIO_H_
#define MAIN_UTIL_GPIO_H_

#include "driver/gpio.h"
#include "esp_system.h"

esp_err_t gpio_pulse(gpio_num_t gpio_num, uint32_t level, uint32_t pulseWidth, uint32_t delayAfter);

#endif /* MAIN_UTIL_GPIO_H_ */

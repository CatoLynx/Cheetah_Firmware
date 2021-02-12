/*
 * Utility functions for GPIO pins
 */

#include "util_gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t gpio_pulse(gpio_num_t gpio_num, uint8_t level, uint32_t pulseWidth, uint32_t delayAfter) {
	esp_err_t err;
	err = gpio_set_level(gpio_num, level);
	if(err != ESP_OK) return err;
	ets_delay_us(pulseWidth);
	err = gpio_set_level(gpio_num, !level);
	if(delayAfter > 0) ets_delay_us(delayAfter);
	return err;
}

esp_err_t gpio_pulse_inv(gpio_num_t gpio_num, uint8_t level, uint32_t pulseWidth, uint32_t delayAfter, uint8_t activeLow) {
	return gpio_pulse(gpio_num, activeLow ? !level : level, pulseWidth, delayAfter);
}

esp_err_t gpio_set(gpio_num_t gpio_num, uint8_t level, uint8_t activeLow) {
	return gpio_set_level(gpio_num, activeLow ? !level : level);
}

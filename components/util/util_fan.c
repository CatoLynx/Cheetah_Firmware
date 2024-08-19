#include "util_fan.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"


#if defined(CONFIG_FAN_ENABLED)


ledc_timer_config_t fan_timer = {
    .duty_resolution = LEDC_TIMER_8_BIT,
    .freq_hz = CONFIG_FAN_PWM_FREQ,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_num = CONFIG_FAN_PWM_TIMER_NUM,
    .clk_cfg = LEDC_AUTO_CLK
};

ledc_channel_config_t fan_channel = {
    .channel    = CONFIG_FAN_PWM_CHANNEL_NUM,
    .duty       = 0,
    .gpio_num   = CONFIG_FAN_PWM_GPIO_NUM,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_sel  = CONFIG_FAN_PWM_TIMER_NUM
};

uint8_t fan_targetSpeed = 0;
uint8_t fan_currentSpeed = 0;
uint8_t fan_speedUpdateRequired = 0;


esp_err_t fan_init() {
    esp_err_t ret;

    ret = ledc_timer_config(&fan_timer);
    if (ret != ESP_OK) return ret;

    ret = ledc_channel_config(&fan_channel);
    if (ret != ESP_OK) return ret;

    ret = fan_set_speed(0);
    return ret;
}

esp_err_t fan_set_speed(uint8_t speed) {
    esp_err_t ret;

    ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, CONFIG_FAN_PWM_CHANNEL_NUM, speed);
    if (ret != ESP_OK) return ret;

    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, CONFIG_FAN_PWM_CHANNEL_NUM);
    return ret;
}

void fan_set_target_speed(uint8_t speed) {
    fan_targetSpeed = speed;
}

void fan_speed_adjust_task(void* arg) {
    // Gradually adjust the fan speed to match the target speed

    while (1) {
        if (fan_currentSpeed < fan_targetSpeed) {
            fan_currentSpeed++;
            fan_speedUpdateRequired = 1;
        } else if (fan_currentSpeed > fan_targetSpeed) {
            fan_currentSpeed--;
            fan_speedUpdateRequired = 1;
        } else {
            fan_speedUpdateRequired = 0;
        }

        if (fan_speedUpdateRequired) {
            fan_set_speed(fan_currentSpeed);
        }

        if (CONFIG_FAN_SPEED_ADJUST_DELAY < 10) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(CONFIG_FAN_SPEED_ADJUST_DELAY / portTICK_PERIOD_MS);
        }
    }
}

#endif
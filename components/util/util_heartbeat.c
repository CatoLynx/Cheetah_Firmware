#include "util_heartbeat.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"


#if defined(CONFIG_HEARTBEAT_ENABLED)


ledc_timer_config_t heartbeat_timer = {
    .duty_resolution = LEDC_TIMER_8_BIT,
    .freq_hz = CONFIG_HEARTBEAT_FREQ,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_num = CONFIG_HEARTBEAT_TIMER_NUM,
    .clk_cfg = LEDC_AUTO_CLK
};

ledc_channel_config_t heartbeat_channel = {
    .channel    = CONFIG_HEARTBEAT_CHANNEL_NUM,
    .duty       = 0,
    .gpio_num   = CONFIG_HEARTBEAT_GPIO_NUM,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_sel  = CONFIG_HEARTBEAT_TIMER_NUM
};


esp_err_t heartbeat_init() {
    ESP_ERROR_CHECK(ledc_timer_config(&heartbeat_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&heartbeat_channel));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, CONFIG_HEARTBEAT_CHANNEL_NUM, CONFIG_HEARTBEAT_DUTY_CYCLE));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, CONFIG_HEARTBEAT_CHANNEL_NUM));
    return ESP_OK;
}

#endif
#include "util_heartbeat.h"

#include "driver/gpio.h"
#include "driver/gptimer.h"


#if defined(CONFIG_HEARTBEAT_ENABLED)

static bool heartbeat_gpioState = false;

gptimer_handle_t gptimer = NULL;
gptimer_config_t timer_config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = 1000, // 1000 Hz => 1 tick = 1 ms
};
gptimer_alarm_config_t alarm_config = {
    .reload_count = 0, // Value to load on alarm
    .alarm_count = CONFIG_HEARTBEAT_PERIOD_MS / 2,
    .flags.auto_reload_on_alarm = true, // Enable auto-reload
};


static bool heartbeat_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    heartbeat_gpioState = !heartbeat_gpioState;
    gpio_set_level(CONFIG_HEARTBEAT_GPIO_NUM, heartbeat_gpioState);
    return false; // No high-priority task was awoken by the event
}

gptimer_event_callbacks_t callbacks = {
    .on_alarm = heartbeat_callback,
};
esp_err_t heartbeat_init() {
    gpio_reset_pin(CONFIG_HEARTBEAT_GPIO_NUM);
    gpio_set_direction(CONFIG_HEARTBEAT_GPIO_NUM, GPIO_MODE_OUTPUT);
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &callbacks, NULL));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));
    return ESP_OK;
}

#endif
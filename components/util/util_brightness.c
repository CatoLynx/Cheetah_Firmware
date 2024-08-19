#include "util_brightness.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_continuous.h"
#include "util_generic.h"


#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL) && defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_SENSOR)

#define LOG_TAG "BRT-SENS"


static uint8_t* brightness;
static uint8_t* baseBrightness;

// Variables for rolling average
#define NUM_READINGS 500
static int16_t brightness_readings[NUM_READINGS];
static uint16_t brightness_readIndex = 0;
static int32_t brightness_total = 0;
static int16_t brightness_average = 0;

esp_err_t brightness_sensor_init(uint8_t* brightnessValue, uint8_t* baseBrightnessValue) {
    esp_err_t ret = ESP_OK;

    brightness = brightnessValue;
    baseBrightness = baseBrightnessValue;

    ret = adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(CONFIG_DISPLAY_BRIGHTNESS_SENSOR_ADC_CHANNEL, ADC_ATTEN_11db);

    // Initialize the rolling average array with the first sensor reading
    int16_t initialReading = adc1_get_raw(CONFIG_DISPLAY_BRIGHTNESS_SENSOR_ADC_CHANNEL);

    for (int i = 0; i < NUM_READINGS; i++) {
        brightness_readings[i] = initialReading;
        brightness_total += brightness_readings[i];
    }
    brightness_average = brightness_total / NUM_READINGS;
    return ret;
}

void brightness_adjust_task(void* arg) {
    // Gradually adjust the brightness based on the sensor value

    while (1) {
        int16_t adcValue = adc1_get_raw(CONFIG_DISPLAY_BRIGHTNESS_SENSOR_ADC_CHANNEL);

        brightness_total -= brightness_readings[brightness_readIndex];
        brightness_readings[brightness_readIndex] = adcValue;
        brightness_total += brightness_readings[brightness_readIndex];
        brightness_readIndex = (brightness_readIndex + 1) % NUM_READINGS;
        brightness_average = brightness_total / NUM_READINGS;

        int32_t adcAverage = brightness_average * 3300 / 4096;
        int32_t outputBrightness = map_int32(adcAverage, CONFIG_DISPLAY_BRIGHTNESS_SENSOR_DARK_VOLTAGE, CONFIG_DISPLAY_BRIGHTNESS_SENSOR_BRIGHT_VOLTAGE, CONFIG_DISPLAY_BRIGHTNESS_SENSOR_MIN_OUT_VAL, CONFIG_DISPLAY_BRIGHTNESS_SENSOR_MAX_OUT_VAL);
        outputBrightness += *baseBrightness - 127;
        if (outputBrightness < CONFIG_DISPLAY_BRIGHTNESS_SENSOR_MIN_OUT_VAL) outputBrightness = CONFIG_DISPLAY_BRIGHTNESS_SENSOR_MIN_OUT_VAL;
        if (outputBrightness > CONFIG_DISPLAY_BRIGHTNESS_SENSOR_MAX_OUT_VAL) outputBrightness = CONFIG_DISPLAY_BRIGHTNESS_SENSOR_MAX_OUT_VAL;
        ESP_LOGV(LOG_TAG, "ADC value: %d, average voltage: %d mV, output brightness: %d", adcValue, adcAverage, outputBrightness);
        *brightness = (uint8_t)outputBrightness;

        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

#endif
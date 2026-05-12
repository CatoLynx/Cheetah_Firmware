#include "util_brightness.h"

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "util_generic.h"


#define LOG_TAG "BRT-CTRL"

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_SENSOR)

static uint8_t* brightness;
static uint8_t* baseBrightness;

// Variables for rolling average
#define NUM_READINGS 500
static int16_t brightness_readings[NUM_READINGS];
static uint16_t brightness_readIndex = 0;
static int32_t brightness_total = 0;
static int16_t brightness_average = 0;

static adc_oneshot_unit_handle_t adc1_handle;

esp_err_t brightness_sensor_init(uint8_t* brightnessValue, uint8_t* baseBrightnessValue) {
    esp_err_t ret = ESP_OK;

    brightness = brightnessValue;
    baseBrightness = baseBrightnessValue;

    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, CONFIG_DISPLAY_BRIGHTNESS_SENSOR_ADC_CHANNEL, &config));

    // Initialize the rolling average array with the first sensor reading
    int initialReading;
    adc_oneshot_read(adc1_handle, CONFIG_DISPLAY_BRIGHTNESS_SENSOR_ADC_CHANNEL, &initialReading);

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
        int adcValue;
        adc_oneshot_read(adc1_handle, CONFIG_DISPLAY_BRIGHTNESS_SENSOR_ADC_CHANNEL, &adcValue);

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
        ESP_LOGV(LOG_TAG, "ADC value: %d, average voltage: %ld mV, output brightness: %ld", adcValue, adcAverage, outputBrightness);
        *brightness = (uint8_t)outputBrightness;

        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

#endif

#if defined(CONFIG_BRIGHTNESS_CONTROL_USE_I2C_DAC)
// Generic brightness control using I²C DAC (e.g. 0-10V analog dimming for electronic ballasts)

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t i2c_dac_handle;

esp_err_t brightness_control_init(void) {
    ESP_LOGI(LOG_TAG, "Initializing I2C DAC brightness control");
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1, // auto select
        .scl_io_num = CONFIG_BRIGHTNESS_CONTROL_I2C_DAC_SCL_IO,
        .sda_io_num = CONFIG_BRIGHTNESS_CONTROL_I2C_DAC_SDA_IO,
        .glitch_ignore_cnt = 7,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    i2c_device_config_t i2c_dev_conf = {
        .scl_speed_hz = CONFIG_BRIGHTNESS_CONTROL_I2C_DAC_SPEED,
        .device_address = CONFIG_BRIGHTNESS_CONTROL_I2C_DAC_ADDR,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &i2c_dev_conf, &i2c_dac_handle));
    return ESP_OK;
}

void display_set_brightness(uint8_t brightness) {
    ESP_LOGD(LOG_TAG, "I2C DAC brightness value: %u", brightness);
    uint8_t buf[2];
    #if defined(CONFIG_BRIGHTNESS_CONTROL_I2C_DAC_RESOLUTION_8_BIT)
    buf[0] = 0x00;
    buf[1] = brightness;
    #elif defined(CONFIG_BRIGHTNESS_CONTROL_I2C_DAC_RESOLUTION_10_BIT)
    buf[0] = brightness >> 6;
    buf[1] = brightness << 2;
    #elif defined(CONFIG_BRIGHTNESS_CONTROL_I2C_DAC_RESOLUTION_12_BIT)
    buf[0] = brightness >> 4;
    buf[1] = brightness << 4;
    #endif
    i2c_master_transmit(i2c_dac_handle, buf, 2, -1);
}
#else
esp_err_t brightness_control_init(void) {
    return ESP_OK;
}

void display_set_brightness(uint8_t brightness) {
    
}
#endif

#endif
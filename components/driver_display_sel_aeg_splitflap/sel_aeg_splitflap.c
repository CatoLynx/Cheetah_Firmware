/*
 * Functions for AEG split-flap displays
 */

#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "rom/ets_sys.h"

#include "macros.h"
#include "util_generic.h"
#include "util_gpio.h"
#include "util_disp_selection.h"
#include "sel_aeg_splitflap.h"

#if defined(CONFIG_DISPLAY_DRIVER_SEL_AEG_SPLITFLAP)


#define LOG_TAG "SEL-AEG-SF"

// Always 7 bytes, regardless of unit count or ZACE usage
// Layout: 1 byte for the ZACE data bus and 6 bytes for the 48 outputs (24 motor enables, 24 sensor enables)
// Units 0 to 23 can be addressed directly, units 24 to 71 via ZACE
static uint8_t display_outBuf[AEG_SEL_SPI_OUT_BUF_SIZE] = {0};

// Always 2 bytes
// Layout: 1 byte for the 6 position sensor bits (bits 6 and 7 unused, always 1) and 1 byte for test and control card IO
// Layout of the latter (MSB to LSB): Position selection ones BCD 1, 2, 4, 8, tens 1, 2, 4, test switch
static uint8_t display_inBuf[AEG_SEL_SPI_IN_BUF_SIZE] = {0};

static spi_device_handle_t spi;
static bool spi_transferOngoing = false;

// Which motors are currently active.
// Multiple motors can be active simultaneously.
static bool motorsActive[AEG_SEL_MAX_UNITS] = {false};

// Per-unit timestamp of when the unit was started.
// Used for calculating rotation timeouts.
static int64_t motorStartTimes[AEG_SEL_MAX_UNITS] = {0};

// Current positions of the units.
// Compared with the unit buffer to determine whether a unit needs to rotate.
static uint8_t unitPositions[AEG_SEL_MAX_UNITS] = {0};

/* HARDWARE PIN TO UNIT ID MAPPING
    Motors:     DIRECT   a1  ... a8  =  0 ...  7
                DIRECT   c1  ... c16 =  8 ... 23
                ZACE TOP a1  ... a8  = 24 ... 31
                ZACE TOP c1  ... c16 = 32 ... 47
                ZACE BOT a1  ... a8  = 48 ... 55
                ZACE BOT c1  ... c16 = 56 ... 71
    Sensors:    DIRECT   a9  ... a24 =  0 ... 15
                DIRECT   c17 ... c24 = 16 ... 23
                ZACE TOP a9  ... a24 = 24 ... 39
                ZACE TOP c17 ... c24 = 40 ... 47
                ZACE BOT a9  ... a24 = 48 ... 63
                ZACE BOT c17 ... c24 = 64 ... 71
*/
enum zace_halves {
    ZACE_NONE,
    ZACE_TOP,
    ZACE_BOTTOM
};
// NOTE: ZACE data bus is flipped! Binary literals for ZACE (and ONLY for Zace) are LSB first!
// Map entry layout: {Byte index in output buffer, bit mask to set or OR, ZACE half, ZACE cycle}
static const uint8_t unitIdToBitPosMap_motors[AEG_SEL_MAX_UNITS][4] = {
    // 0...7 = Byte 6, Bits 7...0
    {6, 0b10000000, ZACE_NONE, 0}, {6, 0b01000000, ZACE_NONE, 0}, {6, 0b00100000, ZACE_NONE, 0}, {6, 0b00010000, ZACE_NONE, 0}, {6, 0b00001000, ZACE_NONE, 0}, {6, 0b00000100, ZACE_NONE, 0}, {6, 0b00000010, ZACE_NONE, 0}, {6, 0b00000001, ZACE_NONE, 0},
    // 8...15 = Byte 5, Bits 7...0
    {5, 0b10000000, ZACE_NONE, 0}, {5, 0b01000000, ZACE_NONE, 0}, {5, 0b00100000, ZACE_NONE, 0}, {5, 0b00010000, ZACE_NONE, 0}, {5, 0b00001000, ZACE_NONE, 0}, {5, 0b00000100, ZACE_NONE, 0}, {5, 0b00000010, ZACE_NONE, 0}, {5, 0b00000001, ZACE_NONE, 0},
    // 16...23 = Byte 3, Bits 7...0
    {3, 0b10000000, ZACE_NONE, 0}, {3, 0b01000000, ZACE_NONE, 0}, {3, 0b00100000, ZACE_NONE, 0}, {3, 0b00010000, ZACE_NONE, 0}, {3, 0b00001000, ZACE_NONE, 0}, {3, 0b00000100, ZACE_NONE, 0}, {3, 0b00000010, ZACE_NONE, 0}, {3, 0b00000001, ZACE_NONE, 0},
    // 24...31 = Byte 0, ZACE top cycle 2, Bits 7...0
    {0, 0b00000001, ZACE_TOP, 2}, {0, 0b00000010, ZACE_TOP, 2}, {0, 0b00000100, ZACE_TOP, 2}, {0, 0b00001000, ZACE_TOP, 2}, {0, 0b00010000, ZACE_TOP, 2}, {0, 0b00100000, ZACE_TOP, 2}, {0, 0b01000000, ZACE_TOP, 2}, {0, 0b10000000, ZACE_TOP, 2},
    // 32...39 = Byte 0, ZACE top cycle 1, Bits 7...0
    {0, 0b00000001, ZACE_TOP, 1}, {0, 0b00000010, ZACE_TOP, 1}, {0, 0b00000100, ZACE_TOP, 1}, {0, 0b00001000, ZACE_TOP, 1}, {0, 0b00010000, ZACE_TOP, 1}, {0, 0b00100000, ZACE_TOP, 1}, {0, 0b01000000, ZACE_TOP, 1}, {0, 0b10000000, ZACE_TOP, 1},
    // 40...47 = Byte 0, ZACE top cycle 0, Bits 7...0
    {0, 0b00000001, ZACE_TOP, 0}, {0, 0b00000010, ZACE_TOP, 0}, {0, 0b00000100, ZACE_TOP, 0}, {0, 0b00001000, ZACE_TOP, 0}, {0, 0b00010000, ZACE_TOP, 0}, {0, 0b00100000, ZACE_TOP, 0}, {0, 0b01000000, ZACE_TOP, 0}, {0, 0b10000000, ZACE_TOP, 0},
    // 24...31 = Byte 0, ZACE bottom cycle 2, Bits 7...0
    {0, 0b00000001, ZACE_BOTTOM, 2}, {0, 0b00000010, ZACE_BOTTOM, 2}, {0, 0b00000100, ZACE_BOTTOM, 2}, {0, 0b00001000, ZACE_BOTTOM, 2}, {0, 0b00010000, ZACE_BOTTOM, 2}, {0, 0b00100000, ZACE_BOTTOM, 2}, {0, 0b01000000, ZACE_BOTTOM, 2}, {0, 0b10000000, ZACE_BOTTOM, 2},
    // 32...39 = Byte 0, ZACE bottom cycle 1, Bits 7...0
    {0, 0b00000001, ZACE_BOTTOM, 1}, {0, 0b00000010, ZACE_BOTTOM, 1}, {0, 0b00000100, ZACE_BOTTOM, 1}, {0, 0b00001000, ZACE_BOTTOM, 1}, {0, 0b00010000, ZACE_BOTTOM, 1}, {0, 0b00100000, ZACE_BOTTOM, 1}, {0, 0b01000000, ZACE_BOTTOM, 1}, {0, 0b10000000, ZACE_BOTTOM, 1},
    // 40...47 = Byte 0, ZACE bottom cycle 0, Bits 7...0
    {0, 0b00000001, ZACE_BOTTOM, 0}, {0, 0b00000010, ZACE_BOTTOM, 0}, {0, 0b00000100, ZACE_BOTTOM, 0}, {0, 0b00001000, ZACE_BOTTOM, 0}, {0, 0b00010000, ZACE_BOTTOM, 0}, {0, 0b00100000, ZACE_BOTTOM, 0}, {0, 0b01000000, ZACE_BOTTOM, 0}, {0, 0b10000000, ZACE_BOTTOM, 0},
};
static const uint8_t unitIdToBitPosMap_sensors[AEG_SEL_MAX_UNITS][4] = {
    // 0...7 = Byte 4, Bits 7...0
    {4, 0b10000000, ZACE_NONE, 0}, {4, 0b01000000, ZACE_NONE, 0}, {4, 0b00100000, ZACE_NONE, 0}, {4, 0b00010000, ZACE_NONE, 0}, {4, 0b00001000, ZACE_NONE, 0}, {4, 0b00000100, ZACE_NONE, 0}, {4, 0b00000010, ZACE_NONE, 0}, {4, 0b00000001, ZACE_NONE, 0},
    // 8...15 = Byte 2, Bits 7...0
    {2, 0b10000000, ZACE_NONE, 0}, {2, 0b01000000, ZACE_NONE, 0}, {2, 0b00100000, ZACE_NONE, 0}, {2, 0b00010000, ZACE_NONE, 0}, {2, 0b00001000, ZACE_NONE, 0}, {2, 0b00000100, ZACE_NONE, 0}, {2, 0b00000010, ZACE_NONE, 0}, {2, 0b00000001, ZACE_NONE, 0},
    // 16...23 = Byte 1, Bits 7...0
    {1, 0b10000000, ZACE_NONE, 0}, {1, 0b01000000, ZACE_NONE, 0}, {1, 0b00100000, ZACE_NONE, 0}, {1, 0b00010000, ZACE_NONE, 0}, {1, 0b00001000, ZACE_NONE, 0}, {1, 0b00000100, ZACE_NONE, 0}, {1, 0b00000010, ZACE_NONE, 0}, {1, 0b00000001, ZACE_NONE, 0},
    // 24...31 = Byte 0, ZACE top cycle 3, Decoder 0, BCD values 0...7
    {0, 0b00001100, ZACE_TOP, 3}, {0, 0b10001100, ZACE_TOP, 3}, {0, 0b01001100, ZACE_TOP, 3}, {0, 0b11001100, ZACE_TOP, 3}, {0, 0b00101100, ZACE_TOP, 3}, {0, 0b10101100, ZACE_TOP, 3}, {0, 0b01101100, ZACE_TOP, 3}, {0, 0b11101100, ZACE_TOP, 3},
    // 32...39 = Byte 0, ZACE top cycle 3, Decoder 2, BCD values 0...7
    {0, 0b00011000, ZACE_TOP, 3}, {0, 0b10011000, ZACE_TOP, 3}, {0, 0b01011000, ZACE_TOP, 3}, {0, 0b11011000, ZACE_TOP, 3}, {0, 0b00111000, ZACE_TOP, 3}, {0, 0b10111000, ZACE_TOP, 3}, {0, 0b01111000, ZACE_TOP, 3}, {0, 0b11111000, ZACE_TOP, 3},
    // 40...47 = Byte 0, ZACE top cycle 3, Decoder 1, BCD values 0...7
    {0, 0b00010100, ZACE_TOP, 3}, {0, 0b10010100, ZACE_TOP, 3}, {0, 0b01010100, ZACE_TOP, 3}, {0, 0b11010100, ZACE_TOP, 3}, {0, 0b00110100, ZACE_TOP, 3}, {0, 0b10110100, ZACE_TOP, 3}, {0, 0b01110100, ZACE_TOP, 3}, {0, 0b11110100, ZACE_TOP, 3},
    // 48...55 = Byte 0, ZACE bottom cycle 3, Decoder 0, BCD values 0...7
    {0, 0b00001100, ZACE_BOTTOM, 3}, {0, 0b10001100, ZACE_BOTTOM, 3}, {0, 0b01001100, ZACE_BOTTOM, 3}, {0, 0b11001100, ZACE_BOTTOM, 3}, {0, 0b00101100, ZACE_BOTTOM, 3}, {0, 0b10101100, ZACE_BOTTOM, 3}, {0, 0b01101100, ZACE_BOTTOM, 3}, {0, 0b11101100, ZACE_BOTTOM, 3},
    // 56...63 = Byte 0, ZACE bottom cycle 3, Decoder 2, BCD values 0...7
    {0, 0b00011000, ZACE_BOTTOM, 3}, {0, 0b10011000, ZACE_BOTTOM, 3}, {0, 0b01011000, ZACE_BOTTOM, 3}, {0, 0b11011000, ZACE_BOTTOM, 3}, {0, 0b00111000, ZACE_BOTTOM, 3}, {0, 0b10111000, ZACE_BOTTOM, 3}, {0, 0b01111000, ZACE_BOTTOM, 3}, {0, 0b11111000, ZACE_BOTTOM, 3},
    // 64...71 = Byte 0, ZACE bottom cycle 3, Decoder 1, BCD values 0...7
    {0, 0b00010100, ZACE_BOTTOM, 3}, {0, 0b10010100, ZACE_BOTTOM, 3}, {0, 0b01010100, ZACE_BOTTOM, 3}, {0, 0b11010100, ZACE_BOTTOM, 3}, {0, 0b00110100, ZACE_BOTTOM, 3}, {0, 0b10110100, ZACE_BOTTOM, 3}, {0, 0b01110100, ZACE_BOTTOM, 3}, {0, 0b11110100, ZACE_BOTTOM, 3},
};
// Define these to ensure we can write the correct BCD pattern to ZACE if not using it.
// An all-0 pattern enables some outputs.
#define ZACE_SENSOR_BYTE 0
#define ZACE_SENSOR_CYCLE 3
#define ZACE_SENSOR_OFF_MASK 0b00011100

esp_err_t display_init(nvs_handle_t* nvsHandle, uint8_t* display_framebuf_mask, uint16_t* display_num_units) {
    /*
     * Set up all needed peripherals
     */

    esp_err_t ret;

    ret = display_selection_loadAndParseConfiguration(nvsHandle, display_framebuf_mask, display_num_units, LOG_TAG);
    if (ret != ESP_OK) return ret;
    
    if (CONFIG_AEG_SEL_DATA_OUT_IO >= 0) gpio_reset_pin(CONFIG_AEG_SEL_DATA_OUT_IO);
    if (CONFIG_AEG_SEL_DATA_IN_IO >= 0) gpio_reset_pin(CONFIG_AEG_SEL_DATA_IN_IO);
    if (CONFIG_AEG_SEL_CLOCK_IO >= 0) gpio_reset_pin(CONFIG_AEG_SEL_CLOCK_IO);
    if (CONFIG_AEG_SEL_OUT_LATCH_IO >= 0) gpio_reset_pin(CONFIG_AEG_SEL_OUT_LATCH_IO);
    if (CONFIG_AEG_SEL_IN_LATCH_IO >= 0) gpio_reset_pin(CONFIG_AEG_SEL_IN_LATCH_IO);
    if (CONFIG_AEG_SEL_SET_BUTTON_IO >= 0) gpio_reset_pin(CONFIG_AEG_SEL_SET_BUTTON_IO);

    #if defined(CONFIG_AEG_SEL_USE_ZACE)
    if (CONFIG_AEG_SEL_ZACE_REG_SEL_A0_IO >= 0) gpio_reset_pin(CONFIG_AEG_SEL_ZACE_REG_SEL_A0_IO);
    if (CONFIG_AEG_SEL_ZACE_REG_SEL_A1_IO >= 0) gpio_reset_pin(CONFIG_AEG_SEL_ZACE_REG_SEL_A1_IO);
    if (CONFIG_AEG_SEL_ZACE_REG_SEL_EN_TOP_IO >= 0) gpio_reset_pin(CONFIG_AEG_SEL_ZACE_REG_SEL_EN_TOP_IO);
    if (CONFIG_AEG_SEL_ZACE_REG_SEL_EN_BOTTOM_IO >= 0) gpio_reset_pin(CONFIG_AEG_SEL_ZACE_REG_SEL_EN_BOTTOM_IO);
    #endif

    if (CONFIG_AEG_SEL_DATA_OUT_IO >= 0) gpio_set_direction(CONFIG_AEG_SEL_DATA_OUT_IO, GPIO_MODE_OUTPUT);
    if (CONFIG_AEG_SEL_DATA_IN_IO >= 0) gpio_set_direction(CONFIG_AEG_SEL_DATA_IN_IO, GPIO_MODE_INPUT);
    if (CONFIG_AEG_SEL_CLOCK_IO >= 0) gpio_set_direction(CONFIG_AEG_SEL_CLOCK_IO, GPIO_MODE_OUTPUT);
    if (CONFIG_AEG_SEL_OUT_LATCH_IO >= 0) gpio_set_direction(CONFIG_AEG_SEL_OUT_LATCH_IO, GPIO_MODE_OUTPUT);
    if (CONFIG_AEG_SEL_IN_LATCH_IO >= 0) gpio_set_direction(CONFIG_AEG_SEL_IN_LATCH_IO, GPIO_MODE_OUTPUT);
    if (CONFIG_AEG_SEL_SET_BUTTON_IO >= 0) gpio_set_direction(CONFIG_AEG_SEL_SET_BUTTON_IO, GPIO_MODE_INPUT);

    #if defined(CONFIG_AEG_SEL_USE_ZACE)
    if (CONFIG_AEG_SEL_ZACE_REG_SEL_A0_IO >= 0) gpio_set_direction(CONFIG_AEG_SEL_ZACE_REG_SEL_A0_IO, GPIO_MODE_OUTPUT);
    if (CONFIG_AEG_SEL_ZACE_REG_SEL_A1_IO >= 0) gpio_set_direction(CONFIG_AEG_SEL_ZACE_REG_SEL_A1_IO, GPIO_MODE_OUTPUT);
    if (CONFIG_AEG_SEL_ZACE_REG_SEL_EN_TOP_IO >= 0) gpio_set_direction(CONFIG_AEG_SEL_ZACE_REG_SEL_EN_TOP_IO, GPIO_MODE_OUTPUT);
    if (CONFIG_AEG_SEL_ZACE_REG_SEL_EN_BOTTOM_IO >= 0) gpio_set_direction(CONFIG_AEG_SEL_ZACE_REG_SEL_EN_BOTTOM_IO, GPIO_MODE_OUTPUT);
    #endif

    if (CONFIG_AEG_SEL_SET_BUTTON_IO >= 0) gpio_set_pull_mode(CONFIG_AEG_SEL_SET_BUTTON_IO, GPIO_PULLUP_ONLY);
    gpio_set_level(CONFIG_AEG_SEL_IN_LATCH_IO, 1); // active low
    gpio_set_level(CONFIG_AEG_SEL_ZACE_REG_SEL_EN_TOP_IO, 1); // active low
    gpio_set_level(CONFIG_AEG_SEL_ZACE_REG_SEL_EN_BOTTOM_IO, 1); // active low

    // Init SPI peripheral
    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_AEG_SEL_DATA_IN_IO,
        .mosi_io_num = CONFIG_AEG_SEL_DATA_OUT_IO,
        .sclk_io_num = CONFIG_AEG_SEL_CLOCK_IO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = AEG_SEL_SPI_BUF_SIZE
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = CONFIG_AEG_SEL_SPI_CLOCK,
        .mode = 0,                      // positive clock, data changes on falling edge
        .spics_io_num = -1,             // -1 = not used
        .queue_size = 1,                // max. 1 transaction in queue
        .pre_cb = aeg_sel_splitflap_pre_transfer_cb,
        .post_cb = aeg_sel_splitflap_post_transfer_cb,
    };
    #if defined(CONFIG_AEG_SEL_SPI_HOST_VSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg, 1));
    ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfg, &spi));
    #elif defined(CONFIG_AEG_SEL_SPI_HOST_HSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, 1));
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfg, &spi));
    #endif

    return ESP_OK;
}

static void aeg_sel_splitflap_latch_input(void) {
    // 1 us low pulse
    gpio_pulse(CONFIG_AEG_SEL_IN_LATCH_IO, 0, CONFIG_AEG_SEL_LATCH_PULSE_DURATION, CONFIG_AEG_SEL_LATCH_PULSE_DURATION, CONFIG_AEG_SEL_LATCH_PULSE_DURATION);
}

static void aeg_sel_splitflap_latch_output(void) {
    // 1 us high pulse
    gpio_pulse(CONFIG_AEG_SEL_OUT_LATCH_IO, 1, CONFIG_AEG_SEL_LATCH_PULSE_DURATION, CONFIG_AEG_SEL_LATCH_PULSE_DURATION, CONFIG_AEG_SEL_LATCH_PULSE_DURATION);
}

static void aeg_sel_splitflap_pre_transfer_cb(spi_transaction_t *t) {
    spi_transferOngoing = true;
    aeg_sel_splitflap_latch_input();
}

static void aeg_sel_splitflap_post_transfer_cb(spi_transaction_t *t) {
    aeg_sel_splitflap_latch_output();
    spi_transferOngoing = false;
}

static void aeg_sel_start_unit(uint8_t unitId) {
    if (unitId >= AEG_SEL_MAX_UNITS) return;
    motorsActive[unitId] = true;
    motorStartTimes[unitId] = esp_timer_get_time();
}

static void aeg_sel_stop_unit(uint8_t unitId) {
    if (unitId >= AEG_SEL_MAX_UNITS) return;
    motorsActive[unitId] = false;
    motorStartTimes[unitId] = 0;
}

static esp_err_t aeg_sel_update_registers(void) {
    spi_transaction_t spi_trans = {
        .tx_buffer = display_outBuf,
        .length = AEG_SEL_SPI_BUF_SIZE * 8,
        .rx_buffer = display_inBuf,
        .rxlength = AEG_SEL_SPI_IN_BUF_SIZE * 8
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &spi_trans));
    return ESP_OK;
}

void display_update(uint8_t* unitBuf, uint8_t* prevUnitBuf, size_t unitBufSize, portMUX_TYPE* unitBufLock, uint8_t* display_framebuf_mask, uint16_t display_num_units) {
    // Nothing to do if buffer hasn't changed
    // TODO: This kinda has to go as the update function must always check
    // the sensors while at least one unit is rotating.
    // Could MAYBE just check while no units are rotating.
    //if (prevUnitBuf != NULL && memcmp(unitBuf, prevUnitBuf, unitBufSize) == 0) return;

    taskENTER_CRITICAL(unitBufLock);
    // TODO: ??? wat do
    //if (prevUnitBuf != NULL) memcpy(prevUnitBuf, unitBuf, unitBufSize);

    for (uint16_t addr = 0; addr < unitBufSize; addr++) {
        if (addr >= AEG_SEL_MAX_UNITS) break;

        // Skip addresses that aren't present
        if (!GET_MASK(display_framebuf_mask, addr)) continue;
        
        // Start/stop units as necessary
        if (unitBuf[addr] != unitPositions[addr] && !motorsActive[addr]) aeg_sel_start_unit(addr);
        else if (motorsActive[addr]) aeg_sel_stop_unit(addr);

        // Do a full register cycle for the current address.
        // To do a full update and multiplex cycle,
        // four shift register writes must be done per ZACE half.
        // This is because of how the ZACE card is connected.
        for (uint8_t cycle = 0; cycle < 8; cycle++) {
            uint8_t currentZaceCycle = cycle % 4;
            uint8_t currentZaceHalf = (cycle < 4) ? ZACE_TOP : ZACE_BOTTOM;
            memset(display_outBuf, 0x00, AEG_SEL_SPI_OUT_BUF_SIZE);

            // Set the registers for the sensor outputs
            uint8_t sensor_byteIdx, sensor_bitMask, sensor_zaceHalf, sensor_zaceCycle;
            if (addr < 24) {
                // The active sensor is within the non-ZACE range
                sensor_byteIdx = unitIdToBitPosMap_sensors[addr][0];
                sensor_bitMask = unitIdToBitPosMap_sensors[addr][1];
                display_outBuf[sensor_byteIdx] = sensor_bitMask;
    #if defined(CONFIG_AEG_SEL_USE_ZACE)
                if (currentZaceCycle == ZACE_SENSOR_CYCLE) {
                    display_outBuf[ZACE_SENSOR_BYTE] = ZACE_SENSOR_OFF_MASK;
                }
    #endif
            }
    #if defined(CONFIG_AEG_SEL_USE_ZACE)
            else {
                // The active sensor is within the ZACE range
                sensor_byteIdx = unitIdToBitPosMap_sensors[addr][0];
                sensor_bitMask = unitIdToBitPosMap_sensors[addr][1];
                sensor_zaceHalf = unitIdToBitPosMap_sensors[addr][2];
                sensor_zaceCycle = unitIdToBitPosMap_sensors[addr][3];
                if (sensor_zaceHalf == currentZaceHalf && sensor_zaceCycle == currentZaceCycle) {
                    display_outBuf[sensor_byteIdx] = sensor_bitMask;
                }
            }
    #endif

            // Set the registers for the motor outputs
            uint8_t motor_byteIdx, motor_bitMask, motor_zaceHalf, motor_zaceCycle;
            int64_t now = esp_timer_get_time();
            for (uint8_t motorIdx = 0; motorIdx < AEG_SEL_MAX_UNITS; motorIdx++) {
                // Check rotation timeout
                if (motorStartTimes[motorIdx] != 0 && ((now - motorStartTimes[motorIdx]) / 1000) >= CONFIG_AEG_SEL_ROTATION_TIMEOUT) {
                    aeg_sel_stop_unit(motorIdx);
                    // TODO: Figure out how to keep timed-out unit from restarting automatically
                }
                if (!motorsActive[motorIdx]) continue;
                
                if (motorIdx < 24) {
                    // The motor index is within the non-ZACE range
                    motor_byteIdx = unitIdToBitPosMap_motors[motorIdx][0];
                    motor_bitMask = unitIdToBitPosMap_motors[motorIdx][1];
                    display_outBuf[motor_byteIdx] |= motor_bitMask;
                }
    #if defined(CONFIG_AEG_SEL_USE_ZACE)
                else {
                    // The motor index is within the ZACE range
                    motor_byteIdx = unitIdToBitPosMap_motors[motorIdx][0];
                    motor_bitMask = unitIdToBitPosMap_motors[motorIdx][1];
                    motor_zaceHalf = unitIdToBitPosMap_motors[motorIdx][2];
                    motor_zaceCycle = unitIdToBitPosMap_motors[motorIdx][3];
                    if (motor_zaceHalf == currentZaceHalf && motor_zaceCycle == currentZaceCycle) {
                        display_outBuf[motor_byteIdx] |= motor_bitMask;
                    }
                }
            }
    #endif

            // Update shift registers
            ESP_ERROR_CHECK(aeg_sel_update_registers());

    #if defined(CONFIG_AEG_SEL_USE_ZACE)
            // ZACE REG_SEL latching
            gpio_set(CONFIG_AEG_SEL_ZACE_REG_SEL_A0_IO, (currentZaceCycle & 1), false);
            gpio_set(CONFIG_AEG_SEL_ZACE_REG_SEL_A1_IO, (currentZaceCycle & 2), false);
            if (currentZaceHalf == ZACE_TOP) gpio_pulse(CONFIG_AEG_SEL_ZACE_REG_SEL_EN_TOP_IO, 1, CONFIG_AEG_SEL_LATCH_PULSE_DURATION, CONFIG_AEG_SEL_LATCH_PULSE_DURATION, CONFIG_AEG_SEL_LATCH_PULSE_DURATION);
            else if (currentZaceHalf == ZACE_BOTTOM) gpio_pulse(CONFIG_AEG_SEL_ZACE_REG_SEL_EN_BOTTOM_IO, 1, CONFIG_AEG_SEL_LATCH_PULSE_DURATION, CONFIG_AEG_SEL_LATCH_PULSE_DURATION, CONFIG_AEG_SEL_LATCH_PULSE_DURATION);
    #endif
        }

        // Do one more cycle to ensure we are getting the sensor inputs from the freshly enabled sensor
        ESP_ERROR_CHECK(aeg_sel_update_registers());

        // Read position of active sensor
        unitPositions[addr] = display_inBuf[0] & 0x3F;
    }

    // TODO: Turn off sensors at the end

    taskEXIT_CRITICAL(unitBufLock);
}

#endif
#pragma once

#include <stdint.h>
#include "esp_event.h"

#define LOG_TAG "WiFi"

void wifi_init_ap(void);
void wifi_init(void);
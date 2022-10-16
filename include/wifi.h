#pragma once

#include <stdint.h>
#include "esp_event.h"
#include "nvs.h"

void wifi_init_ap(void);
void wifi_init(nvs_handle_t* nvsHandle);
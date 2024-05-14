#pragma once

#include "macros.h"
#include <stdint.h>
#include "esp_event.h"
#include "nvs.h"

enum {
    WPA2E_PH2_TLS = 0,
    WPA2E_PH2_PEAP = 1,
    WPA2E_PH2_TTLS = 2
};

enum {
    WPA2E_PH2_TTLS_NONE = 0,
    WPA2E_PH2_TTLS_MSCHAPV2 = 1,
    WPA2E_PH2_TTLS_MSCHAP = 2,
    WPA2E_PH2_TTLS_PAP = 3,
    WPA2E_PH2_TTLS_CHAP = 4
};

void wifi_init_ap(void);
void wifi_init(nvs_handle_t* nvsHandle);
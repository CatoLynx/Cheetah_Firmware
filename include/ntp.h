#include "macros.h"
#include "esp_sntp.h"

void ntp_sync_cb(struct timeval *tv);
void ntp_init(void);
void ntp_sync_time(void);
bool ntp_is_synced(void);
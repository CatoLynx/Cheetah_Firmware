#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "sys/param.h"
#include "cJSON.h"

#include "browser_ota.h"
#include "util_httpd.h"
#include "settings_secret.h"

#define LOG_TAG "BROWSER-OTA"

static uint32_t ota_payload_length = 0;
static uint32_t ota_rx_len = 0;
static uint8_t ota_success = 0;
static httpd_handle_t* ota_server;
static EventGroupHandle_t restart_event_group;
static TaskHandle_t systemRestartTaskHandle;
#define restart_BIT BIT0

// Embedded files - refer to CMakeLists.txt
extern const uint8_t browser_ota_html_start[] asm("_binary_browser_ota_html_start");
extern const uint8_t browser_ota_html_end[]   asm("_binary_browser_ota_html_end");
extern const uint8_t spinner_gif_start[] asm("_binary_spinner_gif_start");
extern const uint8_t spinner_gif_end[]   asm("_binary_spinner_gif_end");


static void systemRestartTask(void* arg) {
    restart_event_group = xEventGroupCreate();
    xEventGroupClearBits(restart_event_group, restart_BIT);
    while (1) {
        EventBits_t staBits = xEventGroupWaitBits(restart_event_group, restart_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        if (!(staBits & restart_BIT)) continue; // In case of timeout
        ESP_LOGI(LOG_TAG, "Restarting in 2 seconds");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        ESP_LOGI(LOG_TAG, "Restarting");
        esp_restart();
    }
}

static esp_err_t ota_get_handler(httpd_req_t *req) {
    // If authenticated == false, the handler already takes care of the server response
    bool authenticated = basic_auth_handler(req, LOG_TAG);
    if (authenticated == false) return ESP_OK;

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)browser_ota_html_start, browser_ota_html_end - browser_ota_html_start);
    return ESP_OK;
}

static esp_err_t ota_spinner_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/gif");
    httpd_resp_send(req, (const char *)spinner_gif_start, spinner_gif_end - spinner_gif_start);
    return ESP_OK;
}

static esp_err_t ota_length_post_handler(httpd_req_t *req) {
    // If authenticated == false, the handler already takes care of the server response
    bool authenticated = basic_auth_handler(req, LOG_TAG);
    if (authenticated == false) return ESP_OK;

    char buf[10];
    size_t length = req->content_len;
    ESP_LOGI(LOG_TAG, "Content length: %d bytes", length);
    
    if (length > 9) {
        return abortRequest(req, "413 Payload Too Large");
    }

    int ret = httpd_req_recv(req, buf, length);
    if (ret <= 0) {
        return abortRequest(req, HTTPD_500);
    }
    buf[length] = 0x00;

    ota_payload_length = atoi(buf);
    ESP_LOGI(LOG_TAG, "OTA payload length: %ld bytes", ota_payload_length);

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t ota_post_handler(httpd_req_t *req) {
    // If authenticated == false, the handler already takes care of the server response
    bool authenticated = basic_auth_handler(req, LOG_TAG);
    if (authenticated == false) return ESP_OK;

    esp_ota_handle_t ota_handle;        // OTA flash handle
    char ota_buf[1024];                 // OTA data buffer for flashing
    size_t length = req->content_len;   // Total POST payload length
    size_t remaining = length;          // Remaining POST payload bytes to be read
    int ret = 0;                        // Number of received bytes per block
    uint8_t req_body_started = 0;       // Whether the form data headers have already been skipped
    uint16_t chunk_length = 0;          // Length of the current OTA chunk

    ota_rx_len = 0;
    ota_success = 0;
    esp_err_t err;

    // Abort if OTA payload length has not been announced
    if(ota_payload_length == 0) {
        ESP_LOGE(LOG_TAG, "Trying to start OTA without announcing OTA payload length, aborting");
        return abortRequest(req, HTTPD_500);
    }

    // OTA partition handle
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if(!update_partition) {
        ESP_LOGE(LOG_TAG, "Failed to find a suitable OTA partition, aborting");
        return abortRequest(req, HTTPD_500);
    }

    ESP_LOGI(LOG_TAG, "Content length: %d bytes", length);

    while (remaining > 0) {
        ret = httpd_req_recv(req, ota_buf, MIN(remaining, sizeof(ota_buf)));
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                ESP_LOGI(LOG_TAG, "Socket timeout, continuing");
                continue;
            }
            ESP_LOGI(LOG_TAG, "Receive error, aborting");
            return abortRequest(req, HTTPD_500);
        }
        remaining -= ret;
        ESP_LOGI(LOG_TAG, "Received %d of %d bytes", (length - remaining), length);

        if(!req_body_started) {
            // We need to wait for the headers to be complete
            // Check if the end of headers is in this block
            char *body_start_p = strstr(ota_buf, "\r\n\r\n") + 4;
            if (!body_start_p) {
                // Header is not yet finished
                continue;
            }
            req_body_started = 1;
            // How many bytes are left to read in this partial OTA payload
            uint32_t body_part_len = ret - (body_start_p - ota_buf);

            if((remaining + body_part_len) < ota_payload_length) {
                // Expected total payload length is smaller than previously announced
                ESP_LOGE(LOG_TAG, "POST payload length is smaller than previously announced OTA payload length, aborting");
                return abortRequest(req, HTTPD_500);
            }

            // Initiate OTA
            err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
            if (err != ESP_OK) {
                ESP_LOGE(LOG_TAG, "Failed to initialise OTA, status %d", err);
                return abortRequest(req, HTTPD_500);
            }
            ESP_LOGI(LOG_TAG, "Writing to partition subtype %d at offset 0x%lx", update_partition->subtype, update_partition->address);

            // Write the first chunk
            chunk_length = MIN((ota_payload_length - ota_rx_len), body_part_len);
            err = esp_ota_write(ota_handle, body_start_p, chunk_length);
            if (err != ESP_OK) {
                ESP_LOGE(LOG_TAG, "Failed to write OTA data, status %d", err);
                return abortRequest(req, HTTPD_500);
            }
            ota_rx_len += chunk_length;
        } else {
            // Header has already been skipped, just receive OTA data until the payload size is reached
            chunk_length = MIN((ota_payload_length - ota_rx_len), ret);
            err = esp_ota_write(ota_handle, ota_buf, chunk_length);
            if (err != ESP_OK) {
                ESP_LOGE(LOG_TAG, "Failed to write OTA data, status %d", err);
                return abortRequest(req, HTTPD_500);
            }
            ota_rx_len += chunk_length;
        }
    }

    // Rx has finished, end OTA
    if (esp_ota_end(ota_handle) == ESP_OK) {
        // Update the boot partition
        if (esp_ota_set_boot_partition(update_partition) == ESP_OK) {
            const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
            ESP_LOGI(LOG_TAG, "Next boot partition subtype %d at offset 0x%lx", boot_partition->subtype, boot_partition->address);
            xEventGroupSetBits(restart_event_group, restart_BIT);
            ota_success = 1;
            ESP_LOGI(LOG_TAG, "OTA success! Restart flag set.");
        } else {
            ESP_LOGE(LOG_TAG, "Failed to set boot partition, aborting");
            return abortRequest(req, HTTPD_500);
        }
    } else {
        ESP_LOGE(LOG_TAG, "Failed to end OTA, aborting");
        return abortRequest(req, HTTPD_500);
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t ota_status_get_handler(httpd_req_t *req) {
    // If authenticated == false, the handler already takes care of the server response
    bool authenticated = basic_auth_handler(req, LOG_TAG);
    if (authenticated == false) return ESP_OK;

    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "payload_length", ota_payload_length);
    cJSON_AddNumberToObject(json, "received", ota_rx_len);
    cJSON_AddBoolToObject  (json, "success", ota_success);

    char *resp = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_Delete(json);
    cJSON_free(resp);
    return ESP_OK;
}

static esp_err_t ota_verify_get_handler(httpd_req_t *req) {
    // If authenticated == false, the handler already takes care of the server response
    bool authenticated = basic_auth_handler(req, LOG_TAG);
    if (authenticated == false) return ESP_OK;

    esp_ota_mark_app_valid_cancel_rollback();
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "OK", 3);
    return ESP_OK;
}

static esp_err_t ota_restart_get_handler(httpd_req_t *req) {
    // If authenticated == false, the handler already takes care of the server response
    bool authenticated = basic_auth_handler(req, LOG_TAG);
    if (authenticated == false) return ESP_OK;

    xEventGroupSetBits(restart_event_group, restart_BIT);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "OK", 3);
    return ESP_OK;
}

static httpd_uri_t ota_get = {
    .uri       = "/ota",
    .method    = HTTP_GET,
    .handler   = ota_get_handler
};

static httpd_uri_t ota_spinner_get = {
    .uri       = "/ota/spinner.gif",
    .method    = HTTP_GET,
    .handler   = ota_spinner_get_handler
};

static httpd_uri_t ota_length_post = {
    .uri       = "/ota/length",
    .method    = HTTP_POST,
    .handler   = ota_length_post_handler
};

static httpd_uri_t ota_post = {
    .uri       = "/ota",
    .method    = HTTP_POST,
    .handler   = ota_post_handler
};

static httpd_uri_t ota_status_get = {
    .uri       = "/ota/status",
    .method    = HTTP_GET,
    .handler   = ota_status_get_handler
};

static httpd_uri_t ota_verify_get = {
    .uri       = "/ota/verify",
    .method    = HTTP_GET,
    .handler   = ota_verify_get_handler
};

static httpd_uri_t ota_restart_get = {
    .uri       = "/ota/restart",
    .method    = HTTP_GET,
    .handler   = ota_restart_get_handler
};

void browser_ota_init(httpd_handle_t* server) {
    ESP_LOGI(LOG_TAG, "Init");
    ESP_LOGI(LOG_TAG, "Registering URI handlers");

    basic_auth_info_t *basic_auth_info = calloc(1, sizeof(basic_auth_info_t));
    basic_auth_info->username = HTTPD_CONFIG_USERNAME;
    basic_auth_info->password = HTTPD_CONFIG_PASSWORD;
    basic_auth_info->realm    = "Cheetah Firmware Update";
    
    ota_get.user_ctx = basic_auth_info;
    ota_length_post.user_ctx = basic_auth_info;
    ota_post.user_ctx = basic_auth_info;
    ota_status_get.user_ctx = basic_auth_info;
    ota_verify_get.user_ctx = basic_auth_info;
    ota_restart_get.user_ctx = basic_auth_info;

    httpd_register_uri_handler(*server, &ota_get);
    httpd_register_uri_handler(*server, &ota_spinner_get);
    httpd_register_uri_handler(*server, &ota_length_post);
    httpd_register_uri_handler(*server, &ota_post);
    httpd_register_uri_handler(*server, &ota_status_get);
    httpd_register_uri_handler(*server, &ota_verify_get);
    httpd_register_uri_handler(*server, &ota_restart_get);
    ota_server = server;
    
    ESP_LOGI(LOG_TAG, "Creating restart task");
    xTaskCreatePinnedToCore(&systemRestartTask, "restartTask", 2048, NULL, 5, &systemRestartTaskHandle, 0);
}

void browser_ota_deinit(void) {
    ESP_LOGI(LOG_TAG, "De-Init");
    ESP_LOGI(LOG_TAG, "Deleting restart task");
    vTaskDelete(systemRestartTaskHandle);
    ESP_LOGI(LOG_TAG, "Unregistering URI handlers");
    httpd_unregister_uri_handler(*ota_server, ota_get.uri, ota_get.method);
    httpd_unregister_uri_handler(*ota_server, ota_spinner_get.uri, ota_spinner_get.method);
    httpd_unregister_uri_handler(*ota_server, ota_length_post.uri, ota_length_post.method);
    httpd_unregister_uri_handler(*ota_server, ota_post.uri, ota_post.method);
    httpd_unregister_uri_handler(*ota_server, ota_status_get.uri, ota_status_get.method);
    httpd_unregister_uri_handler(*ota_server, ota_verify_get.uri, ota_verify_get.method);
    httpd_unregister_uri_handler(*ota_server, ota_restart_get.uri, ota_restart_get.method);
}
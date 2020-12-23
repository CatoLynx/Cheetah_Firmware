#include "esp_log.h"
#include "esp_ota_ops.h"
#include "sys/param.h"

#include "browser_ota.h"

#define LOG_TAG "OTA-HTTPD"

static uint32_t ota_payload_length = 0;

// Embedded files - refer to CMakeLists.txt
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[]   asm("_binary_favicon_ico_end");
extern const uint8_t jquery_3_4_1_min_js_start[] asm("_binary_jquery_3_4_1_min_js_start");
extern const uint8_t jquery_3_4_1_min_js_end[]   asm("_binary_jquery_3_4_1_min_js_end");


static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
	return ESP_OK;
}

static esp_err_t favicon_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/x-icon");
	httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);
	return ESP_OK;
}

static esp_err_t jquery_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)jquery_3_4_1_min_js_start, (jquery_3_4_1_min_js_end - jquery_3_4_1_min_js_start)-1);
	return ESP_OK;
}

static esp_err_t ota_length_post_handler(httpd_req_t *req) {
    char buf[10];
    size_t length = req->content_len;
    if (length > 9) {
        return ESP_FAIL;
    }

    ESP_LOGI(LOG_TAG, "Content length: %d bytes", length);

    int ret = httpd_req_recv(req, buf, length);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    buf[length] = 0x00;

    ota_payload_length = atoi(buf);
    ESP_LOGI(LOG_TAG, "OTA payload length: %d bytes", ota_payload_length);

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t ota_post_handler(httpd_req_t *req) {
    esp_ota_handle_t ota_handle;        // OTA flash handle
    char ota_buf[1024];                 // OTA data buffer for flashing
    size_t length = req->content_len;   // Total POST payload length
    size_t remaining = length;          // Remaining POST payload bytes to be read
    int ret = 0;                        // Number of received bytes per block
    uint8_t req_body_started = 0;       // Whether the form data headers have already been skipped
    uint32_t ota_rx_len = 0;            // How many bytes of the OTA payload have already been received
    uint16_t chunk_length = 0;          // Length of the current OTA chunk

    // Abort if OTA payload length has not been announed
    if(ota_payload_length == 0) {
        ESP_LOGE(LOG_TAG, "Trying to start OTA without announcing OTA payload length, aborting");
        return ESP_FAIL;
    }

    // OTA partition handle
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if(!update_partition) {
        ESP_LOGE(LOG_TAG, "Failed to find a suitable OTA partition, aborting");
        return ESP_FAIL;
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
            return ESP_FAIL;
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
                return ESP_FAIL;
            }

            // Initiate OTA
            esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
            if (err != ESP_OK) {
                ESP_LOGE(LOG_TAG, "Failed to initialise OTA, status %d", err);
                return ESP_FAIL;
            }
            ESP_LOGI(LOG_TAG, "Writing to partition subtype %d at offset 0x%x", update_partition->subtype, update_partition->address);

            // Write the first chunk
            chunk_length = MIN((ota_payload_length - ota_rx_len), body_part_len);
            esp_ota_write(ota_handle, body_start_p, chunk_length);
            ota_rx_len += chunk_length;
        } else {
            // Header has already been skipped, just receive OTA data until the payload size is reached
            chunk_length = MIN((ota_payload_length - ota_rx_len), ret);
            esp_ota_write(ota_handle, ota_buf, chunk_length);
            ota_rx_len += chunk_length;
        }
    }

    // Rx has finished, end OTA
    if (esp_ota_end(ota_handle) == ESP_OK) {
        // Update the boot partition
        if (esp_ota_set_boot_partition(update_partition) == ESP_OK) {
            const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
            ESP_LOGI(LOG_TAG, "Next boot partition subtype %d at offset 0x%x", boot_partition->subtype, boot_partition->address);
            ESP_LOGI(LOG_TAG, "OTA success! Waiting for restart...");
        } else {
            ESP_LOGE(LOG_TAG, "Failed to set boot partition, aborting");
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(LOG_TAG, "Failed to end OTA, aborting");
        return ESP_FAIL;
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t root_get = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler
};

static const httpd_uri_t favicon_get = {
    .uri       = "/favicon.ico",
    .method    = HTTP_GET,
    .handler   = favicon_get_handler
};

static const httpd_uri_t jquery_get = {
    .uri       = "/jquery-3.4.1.min.js",
    .method    = HTTP_GET,
    .handler   = jquery_get_handler
};

static const httpd_uri_t ota_length_post = {
    .uri       = "/ota/length",
    .method    = HTTP_POST,
    .handler   = ota_length_post_handler
};

static const httpd_uri_t ota_post = {
    .uri       = "/ota",
    .method    = HTTP_POST,
    .handler   = ota_post_handler
};

httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(LOG_TAG, "Starting HTTP server on port %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(LOG_TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root_get);
        httpd_register_uri_handler(server, &favicon_get);
        httpd_register_uri_handler(server, &jquery_get);
        httpd_register_uri_handler(server, &ota_length_post);
        httpd_register_uri_handler(server, &ota_post);
        return server;
    }

    ESP_LOGI(LOG_TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server) {
    httpd_stop(server);
}
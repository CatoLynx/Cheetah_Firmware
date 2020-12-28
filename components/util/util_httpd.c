#include "esp_log.h"
#include <string.h>
#include "sys/param.h"

#include "util_httpd.h"


esp_err_t abortRequest(httpd_req_t *req, const char* message) {
    httpd_resp_set_status(req, message);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t post_recv_handler(const char* log_tag, httpd_req_t *req, uint8_t* dest, uint32_t max_size) {
    char recv_buf[1024];                // Data buffer for receiving
    size_t length = req->content_len;   // Total POST payload length
    size_t remaining = length;          // Remaining POST payload bytes to be read
    int ret = 0;                        // Number of received bytes per block
    uint8_t req_body_started = 0;       // Whether the form data headers have already been skipped
    uint16_t chunk_length = 0;          // Length of the current chunk
    uint16_t rx_len = 0;                // Total payload bytes received
    ESP_LOGI(log_tag, "Content length: %d bytes", length);

    while (remaining > 0) {
        ret = httpd_req_recv(req, recv_buf, MIN(remaining, sizeof(recv_buf)));
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                ESP_LOGI(log_tag, "Socket timeout, continuing");
                continue;
            }
            ESP_LOGI(log_tag, "Receive error, aborting");
            return abortRequest(req, "500 Internal Server Error");
        }
        remaining -= ret;
        ESP_LOGI(log_tag, "Received %d of %d bytes", (length - remaining), length);

        if(!req_body_started) {
            // We need to wait for the headers to be complete
            // Check if the end of headers is in this block
            char *body_start_p = strstr(recv_buf, "\r\n\r\n") + 4;
            ESP_LOGI(log_tag, "Body starts at offset %d", (body_start_p - recv_buf));
            if (!body_start_p) {
                // Header is not yet finished
                continue;
            }
            req_body_started = 1;
            // How many bytes are left to read in this partial payload
            uint32_t body_part_len = ret - (body_start_p - recv_buf);

            // Write the first chunk
            chunk_length = MAX(MIN((max_size - rx_len), body_part_len), 0);
            ESP_LOGI(log_tag, "Copying chunk of length %d", chunk_length);
            memcpy(dest, body_start_p, chunk_length);
            rx_len += chunk_length;
        } else {
            // Header has already been skipped, just receive data until the payload size is reached
            chunk_length = MAX(MIN((max_size - rx_len), ret), 0);
            ESP_LOGI(log_tag, "Copying chunk of length %d", chunk_length);
            memcpy(dest, recv_buf, chunk_length);
            rx_len += chunk_length;
        }
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
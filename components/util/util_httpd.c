#include "esp_log.h"
#include <string.h>
#include "sys/param.h"
#include "esp_tls_crypto.h"

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
    ESP_LOGD(log_tag, "Content length: %d bytes", length);

    while (remaining > 0) {
        ret = httpd_req_recv(req, recv_buf, MIN(remaining, sizeof(recv_buf)));
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                ESP_LOGI(log_tag, "Socket timeout, continuing");
                continue;
            }
            ESP_LOGE(log_tag, "Receive error, aborting");
            return abortRequest(req, HTTPD_500);
        }
        remaining -= ret;
        ESP_LOGI(log_tag, "Received %d of %d bytes", (length - remaining), length);

        if(!req_body_started) {
            // We need to wait for the headers to be complete
            // Check if the end of headers is in this block
            char *body_start_p = strstr(recv_buf, "\r\n\r\n") + 4;
            ESP_LOGD(log_tag, "Body starts at offset %d", (body_start_p - recv_buf));
            if (!body_start_p) {
                // Header is not yet finished
                continue;
            }
            req_body_started = 1;
            // How many bytes are left to read in this partial payload
            uint32_t body_part_len = ret - (body_start_p - recv_buf);

            // Write the first chunk
            chunk_length = MAX(MIN((max_size - rx_len), body_part_len), 0);
            ESP_LOGD(log_tag, "Copying chunk of length %d", chunk_length);
            memcpy(dest, body_start_p, chunk_length);
            rx_len += chunk_length;
        } else {
            // Header has already been skipped, just receive data until the payload size is reached
            chunk_length = MAX(MIN((max_size - rx_len), ret), 0);
            ESP_LOGD(log_tag, "Copying chunk of length %d", chunk_length);
            memcpy(dest + rx_len, recv_buf, chunk_length);
            rx_len += chunk_length;
        }
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

char* http_auth_basic_digest(const char *username, const char *password) {
    int out;
    char *user_info = NULL;
    char *digest = NULL;
    size_t n = 0;
    asprintf(&user_info, "%s:%s", username, password);
    esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));
    digest = calloc(1, 6 + n + 1);
    strcpy(digest, "Basic ");
    esp_crypto_base64_encode((unsigned char *)digest + 6, n, (size_t *)&out, (const unsigned char *)user_info, strlen(user_info));
    free(user_info);
    return digest;
}

bool basic_auth_handler(httpd_req_t* req, const char* log_tag) {
    char* buf = NULL;
    size_t buf_len = 0;
    bool authenticated = false;

    // If no auth data is available, assume authenticated
    if (req->user_ctx == NULL) return true;

    basic_auth_info_t* basic_auth_info = req->user_ctx;
    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;

    char* auth_hdr;
    int ret = asprintf(&auth_hdr, "Basic realm=\"%s\"", basic_auth_info->realm);
    if (ret == -1) {
        // asprintf error
        ESP_LOGE(log_tag, "asprintf call failed!");
        httpd_resp_set_status(req, HTTPD_500);
        httpd_resp_send(req, "Internal Server Error", HTTPD_RESP_USE_STRLEN);
        return false;
    }

    if (buf_len > 1) {
        buf = calloc(1, buf_len);
        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
            ESP_LOGI(log_tag, "Found header => Authorization: %s", buf);
        } else {
            ESP_LOGE(log_tag, "No auth value received");
        }

        char* auth_credentials = http_auth_basic_digest(basic_auth_info->username, basic_auth_info->password);
        if (strncmp(auth_credentials, buf, buf_len)) {
            ESP_LOGE(log_tag, "Not authenticated");
            httpd_resp_set_status(req, HTTPD_401);
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            httpd_resp_set_hdr(req, "WWW-Authenticate", auth_hdr);
            httpd_resp_send(req, NULL, 0);
        } else {
            ESP_LOGI(log_tag, "Authenticated!");
            authenticated = true;
        }
        free(auth_credentials);
        free(buf);
    } else {
        ESP_LOGE(log_tag, "No auth header received");
        httpd_resp_set_status(req, HTTPD_401);
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_hdr(req, "WWW-Authenticate", auth_hdr);
        httpd_resp_send(req, NULL, 0);
    }
    free(auth_hdr);
    return authenticated;
}
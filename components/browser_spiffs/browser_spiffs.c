#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <sys/unistd.h>
#include "esp_spiffs.h"
#include "sys/param.h"
#include "dirent.h"
#include "cJSON.h"

#include "browser_spiffs.h"
#include "util_httpd.h"
#include "util_generic.h"
#include "settings_secret.h"

#define LOG_TAG "BROWSER-SPIFFS"

#define MAX_FILENAME_LENGTH 12 // 8.3 filename

static httpd_handle_t* spiffs_server;
static uint32_t upload_payload_length = 0;
static uint32_t upload_rx_len = 0;
static uint8_t upload_success = 0;
static char upload_filename[MAX_FILENAME_LENGTH + 1] = { 0x00 };

// Embedded files - refer to CMakeLists.txt
extern const uint8_t browser_spiffs_html_start[] asm("_binary_browser_spiffs_html_start");
extern const uint8_t browser_spiffs_html_end[]   asm("_binary_browser_spiffs_html_end");


static esp_err_t spiffs_get_handler(httpd_req_t *req) {
    // If authenticated == false, the handler already takes care of the server response
    bool authenticated = basic_auth_handler(req, LOG_TAG);
    if (authenticated == false) return ESP_OK;
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)browser_spiffs_html_start, browser_spiffs_html_end - browser_spiffs_html_start);
    return ESP_OK;
}

static esp_err_t spiffs_files_get_handler(httpd_req_t *req) {
    // If authenticated == false, the handler already takes care of the server response
    bool authenticated = basic_auth_handler(req, LOG_TAG);
    if (authenticated == false) return ESP_OK;

    cJSON* json = cJSON_CreateObject();
    cJSON* arr = cJSON_CreateArray();

    DIR *d;
    struct dirent *dir;
    d = opendir("/spiffs/");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            cJSON* str_entry = cJSON_CreateString(dir->d_name);
            cJSON_AddItemToArray(arr, str_entry);
        }
        closedir(d);
    }

    cJSON_AddItemToObject(json, "files", arr);

    char *resp = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_Delete(json);
    cJSON_free(resp);
    return ESP_OK;
}

static esp_err_t spiffs_download_get_handler(httpd_req_t *req) {
    // If authenticated == false, the handler already takes care of the server response
    bool authenticated = basic_auth_handler(req, LOG_TAG);
    if (authenticated == false) return ESP_OK;

    size_t qs_len = httpd_req_get_url_query_len(req);
    if (qs_len == 0) {
        return abortRequest(req, HTTPD_404);
    }
    qs_len++; // For null terminator

    char* query_string = malloc(qs_len);
    char file_name[13] = { 0x00 }; // 8.3 filename + null
    httpd_req_get_url_query_str(req, query_string, qs_len);
    esp_err_t status = httpd_query_key_value(query_string, "filename", file_name, 13);
    free(query_string);
    if (status != ESP_OK) {
        return abortRequest(req, HTTPD_500);
    }

    char* disposition_header;
    asprintf(&disposition_header, "attachment; filename=\"%s\"", file_name);
    httpd_resp_set_hdr(req, "Content-Disposition", disposition_header);

    char file_path[21]; // "/spiffs/" + 8.3 filename + null
    snprintf(file_path, 21, "/spiffs/%s", file_name);
    ESP_LOGI(LOG_TAG, "Opening file: %s", file_path);
    FILE* file = fopen(file_path, "r");
    if (file == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to open file");
        return abortRequest(req, HTTPD_500);
    }

    size_t chunk_size = 0;
    char file_buf[1024];

    do {
        /* Read file in chunks into the file buffer */
        chunk_size = fread(file_buf, 1, 1024, file);

        if (chunk_size > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, file_buf, chunk_size) != ESP_OK) {
                fclose(file);
                ESP_LOGE(LOG_TAG, "Failed to send file chunk");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                free(disposition_header);
                return abortRequest(req, HTTPD_500);
           }
        }

        /* Keep looping till the whole file is sent */
    } while (chunk_size != 0);

    // Close connection
    httpd_resp_send_chunk(req, NULL, 0);

    ESP_LOGI(LOG_TAG, "free");
    fclose(file);
    free(disposition_header);
    return ESP_OK;
}

static esp_err_t spiffs_delete_post_handler(httpd_req_t *req) {
    // If authenticated == false, the handler already takes care of the server response
    bool authenticated = basic_auth_handler(req, LOG_TAG);
    if (authenticated == false) return ESP_OK;

    ESP_LOGI(LOG_TAG, "Content length: %d bytes", req->content_len);

    char* buf = malloc(req->content_len);
    httpd_req_recv(req, buf, req->content_len);

    cJSON* json = cJSON_Parse(buf);
    free(buf);

    if (!cJSON_IsObject(json)) {
        cJSON_Delete(json);
        return abortRequest(req, HTTPD_500);
    }

    cJSON* file_name_str = cJSON_GetObjectItem(json, "file_name");
    if (!cJSON_IsString(file_name_str)) {
        cJSON_Delete(json);
        return abortRequest(req, HTTPD_500);
    }
    char* file_name = cJSON_GetStringValue(file_name_str);
    char file_path[21]; // "/spiffs/" + 8.3 filename + null
    snprintf(file_path, 21, "/spiffs/%s", file_name);

    cJSON_Delete(json);

    int status = unlink(file_path);
    if (status == -1) {
        ESP_LOGE(LOG_TAG, "Unlink failed for %s", file_path);
        return abortRequest(req, HTTPD_500);
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t spiffs_upload_metadata_post_handler(httpd_req_t *req) {
    // If authenticated == false, the handler already takes care of the server response
    bool authenticated = basic_auth_handler(req, LOG_TAG);
    if (authenticated == false) return ESP_OK;

    ESP_LOGI(LOG_TAG, "Content length: %d bytes", req->content_len);

    char* buf = malloc(req->content_len);
    httpd_req_recv(req, buf, req->content_len);

    cJSON* json = cJSON_Parse(buf);
    free(buf);

    if (!cJSON_IsObject(json)) {
        cJSON_Delete(json);
        return abortRequest(req, HTTPD_500);
    }

    cJSON* file_name_str = cJSON_GetObjectItem(json, "file_name");
    if (!cJSON_IsString(file_name_str)) {
        cJSON_Delete(json);
        return abortRequest(req, HTTPD_500);
    }
    char* file_name = cJSON_GetStringValue(file_name_str);
    memset(upload_filename, 0x00, MAX_FILENAME_LENGTH + 1);
    strncpy(upload_filename, file_name, MAX_FILENAME_LENGTH);

    cJSON* file_size_int = cJSON_GetObjectItem(json, "file_size");
    if (!cJSON_IsNumber(file_size_int)) {
        cJSON_Delete(json);
        return abortRequest(req, HTTPD_500);
    }
    upload_payload_length = cJSON_GetNumberValue(file_size_int);

    cJSON_Delete(json);

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t spiffs_upload_post_handler(httpd_req_t *req) {
    // If authenticated == false, the handler already takes care of the server response
    bool authenticated = basic_auth_handler(req, LOG_TAG);
    if (authenticated == false) return ESP_OK;

    char file_buf[1024];                // Data buffer for storing the file
    size_t length = req->content_len;   // Total POST payload length
    size_t remaining = length;          // Remaining POST payload bytes to be read
    int ret = 0;                        // Number of received bytes per block
    uint8_t req_body_started = 0;       // Whether the form data headers have already been skipped
    uint16_t chunk_length = 0;          // Length of the current file chunk

    upload_rx_len = 0;
    upload_success = 0;

    // Abort if payload length has not been announced
    if(upload_payload_length == 0) {
        ESP_LOGE(LOG_TAG, "Trying to start file upload without announcing metadata, aborting");
        return abortRequest(req, HTTPD_500);
    }

    ESP_LOGI(LOG_TAG, "Content length: %d bytes", length);

    FILE* file = NULL;

    while (remaining > 0) {
        ret = httpd_req_recv(req, file_buf, MIN(remaining, sizeof(file_buf)));
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
            char *body_start_p = strstr(file_buf, "\r\n\r\n") + 4;
            if (!body_start_p) {
                // Header is not yet finished
                continue;
            }
            req_body_started = 1;
            // How many bytes are left to read in this partial OTA payload
            uint32_t body_part_len = ret - (body_start_p - file_buf);

            if((remaining + body_part_len) < upload_payload_length) {
                // Expected total payload length is smaller than previously announced
                ESP_LOGE(LOG_TAG, "POST payload length is smaller than previously announced payload length, aborting");
                return abortRequest(req, HTTPD_500);
            }

            // Open file
            char filename[21]; // "/spiffs/" + 8.3 filename + null
            snprintf(filename, 21, "/spiffs/%s", upload_filename);
            file = fopen(filename, "w");
            if (file == NULL) {
                ESP_LOGE(LOG_TAG, "Failed to open file");
                return abortRequest(req, HTTPD_500);
            }
            ESP_LOGI(LOG_TAG, "File opened: %s", filename);

            // Write the first chunk
            chunk_length = MIN((upload_payload_length - upload_rx_len), body_part_len);
            size_t write_count = fwrite(body_start_p, 1, chunk_length, file);
            if (write_count != chunk_length) {
                ESP_LOGE(LOG_TAG, "Failed to write to file");
                return abortRequest(req, HTTPD_500);
            }
            upload_rx_len += chunk_length;
        } else {
            // Header has already been skipped, just receive data until the payload size is reached
            chunk_length = MIN((upload_payload_length - upload_rx_len), ret);
            size_t write_count = fwrite(file_buf, 1, chunk_length, file);
            if (write_count != chunk_length) {
                ESP_LOGE(LOG_TAG, "Failed to write to file");
                return abortRequest(req, HTTPD_500);
            }
            upload_rx_len += chunk_length;
        }
    }

    // Rx has finished, close file
    int status = fclose(file);
    if (status != 0) {
        ESP_LOGE(LOG_TAG, "Failed to close file, aborting");
        return abortRequest(req, HTTPD_500);
    }
    ESP_LOGI(LOG_TAG, "File closed");

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static httpd_uri_t spiffs_get = {
    .uri       = "/spiffs",
    .method    = HTTP_GET,
    .handler   = spiffs_get_handler
};

static httpd_uri_t spiffs_files_get = {
    .uri       = "/spiffs/files.json",
    .method    = HTTP_GET,
    .handler   = spiffs_files_get_handler
};

static httpd_uri_t spiffs_download_get = {
    .uri       = "/spiffs/download",
    .method    = HTTP_GET,
    .handler   = spiffs_download_get_handler
};

static httpd_uri_t spiffs_delete_post = {
    .uri       = "/spiffs/delete.json",
    .method    = HTTP_POST,
    .handler   = spiffs_delete_post_handler
};

static httpd_uri_t spiffs_upload_metadata_post = {
    .uri       = "/spiffs/upload-metadata.json",
    .method    = HTTP_POST,
    .handler   = spiffs_upload_metadata_post_handler
};

static httpd_uri_t spiffs_upload_post = {
    .uri       = "/spiffs/upload",
    .method    = HTTP_POST,
    .handler   = spiffs_upload_post_handler
};

void browser_spiffs_init(httpd_handle_t* server) {
    ESP_LOGI(LOG_TAG, "Init");
    ESP_LOGI(LOG_TAG, "Registering URI handlers");

    basic_auth_info_t *basic_auth_info = calloc(1, sizeof(basic_auth_info_t));
    basic_auth_info->username = HTTPD_CONFIG_USERNAME;
    basic_auth_info->password = HTTPD_CONFIG_PASSWORD;
    basic_auth_info->realm    = "ESP32 Configuration";
    
    spiffs_get.user_ctx = basic_auth_info;
    spiffs_files_get.user_ctx = basic_auth_info;
    spiffs_download_get.user_ctx = basic_auth_info;
    spiffs_delete_post.user_ctx = basic_auth_info;
    spiffs_upload_metadata_post.user_ctx = basic_auth_info;
    spiffs_upload_post.user_ctx = basic_auth_info;

    httpd_register_uri_handler(*server, &spiffs_get);
    httpd_register_uri_handler(*server, &spiffs_files_get);
    httpd_register_uri_handler(*server, &spiffs_download_get);
    httpd_register_uri_handler(*server, &spiffs_delete_post);
    httpd_register_uri_handler(*server, &spiffs_upload_metadata_post);
    httpd_register_uri_handler(*server, &spiffs_upload_post);
    spiffs_server = server;
}

void browser_spiffs_deinit(void) {
    ESP_LOGI(LOG_TAG, "De-Init");
    ESP_LOGI(LOG_TAG, "Unregistering URI handlers");
    httpd_unregister_uri_handler(*spiffs_server, spiffs_get.uri, spiffs_get.method);
    httpd_unregister_uri_handler(*spiffs_server, spiffs_files_get.uri, spiffs_files_get.method);
    httpd_unregister_uri_handler(*spiffs_server, spiffs_download_get.uri, spiffs_download_get.method);
    httpd_unregister_uri_handler(*spiffs_server, spiffs_delete_post.uri, spiffs_delete_post.method);
    httpd_unregister_uri_handler(*spiffs_server, spiffs_upload_metadata_post.uri, spiffs_upload_metadata_post.method);
    httpd_unregister_uri_handler(*spiffs_server, spiffs_upload_post.uri, spiffs_upload_post.method);
}
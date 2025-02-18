#include "util_buffer.h"
#include "util_generic.h"
#include "macros.h"
#include "esp_log.h"
#include "mbedtls/base64.h"
#include "sys/param.h"
#include <string.h>


void buffer_8to1(uint8_t* buf8, uint8_t* buf1, uint16_t width, uint16_t height, buf_merge_t mergeType) {
    // Convert an 8bpp buffer into a 1bpp buffer
    uint32_t buf1ByteIdx;
    uint8_t buf1BitIdx;
    uint32_t buf8Idx;
    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            buf1ByteIdx = x * DIV_CEIL(height, 8) + y / 8;
            buf1BitIdx = y % 8;
            buf8Idx = x * height + y;

            // Data bit
            if (buf8[buf8Idx] > 127) {
                if (mergeType == MT_OVERWRITE || mergeType == MT_KEEP_1) buf1[buf1ByteIdx] |= (1 << buf1BitIdx);
            } else {
                if (mergeType == MT_OVERWRITE || mergeType == MT_KEEP_0) buf1[buf1ByteIdx] &= ~(1 << buf1BitIdx);
            }
        }
    }
}

void buffer_utf8_to_iso88591(char* dst, char* src) {
    if (src == NULL || dst == NULL) return;

    uint32_t codepoint = 0;
    while (*src) {
        unsigned char ch = (unsigned char)*src;
        if (ch <= 0x7f) codepoint = ch;
        else if (ch <= 0xbf) codepoint = (codepoint << 6) | (ch & 0x3f);
        else if (ch <= 0xdf) codepoint = ch & 0x1f;
        else if (ch <= 0xef) codepoint = ch & 0x0f;
        else codepoint = ch & 0x07;
        src++;
        if (((*src & 0xc0) != 0x80) && (codepoint <= 0x10ffff)) {
            if (codepoint <= 0xFF) {
                *dst = (char)codepoint;
                dst++;
            } else {
                // Character not in range [0 ... 255], skip
            }
        }
    }
}

void buffer_iso88591_to_utf8(char* dst, char* src) {
    if (src == NULL || dst == NULL) return;
    
    while (*src) {
        if (*src <= 0x7f) {
            *dst++ = *src++;
        } else {
            *dst++ = 0xc2 + (*src > 0xbf);
            *dst++ = (*src++ & 0x3f) + 0x80;
        }
    }
}

#if defined(DISPLAY_HAS_TEXT_BUFFER)
void buffer_textbuf_to_charbuf(uint8_t* display_text_buffer, uint8_t* display_char_buffer, uint16_t* display_quirk_flags_buffer, uint16_t textBufSize, uint16_t charBufSize) {
     /*
     Convert a text buffer to a character buffer and a quirk flag buffer.
     The character buffer will contain the base characters for the display,
     the quirk flags buffer will contain quirk flags per character,
     such as a combining full stop.
     This function also handles line breaks.
     */

    uint16_t charBufIndex = 0;
    uint16_t charBufCol = 0;
    uint16_t charBufRow = 0;
    uint8_t incrementCharBufIndex = 0; // Temporary flag to store whether a character was added to the char buffer
    uint8_t characterHandlingCompleted = 0; // Temporary flag to tellwhether a character needs to be handles further

    // Reset the char buffer to user-defined character and the quirk flags buffer to 0x00
    memset(display_char_buffer, CONFIG_DISPLAY_CHAR_BUF_INIT_VALUE, charBufSize);
    memset(display_quirk_flags_buffer, 0x00, charBufSize * 2);

    for (uint16_t textBufIndex = 0; textBufIndex < textBufSize; textBufIndex++) {
        // Stop at null byte
        if (display_text_buffer[textBufIndex] == 0) return;

        incrementCharBufIndex = 0;
        characterHandlingCompleted = 0;

        #if defined(CONFIG_DISPLAY_QUIRKS_COMBINING_FULL_STOP)
        if (display_text_buffer[textBufIndex] == '.') {
            /*
            If a full stop is found:
            
            - If this is the first character in the text buffer, set this index's full stop flag and increment charBufIndex
            - If this is not the first character, but the previous character was already a full stop, set this index's full stop flag and increment charBufIndex
            - If this is not the first character and the previous character was not a full stop, set the previous index's full stop flag and do not increase charBufIndex
            */

            if (textBufIndex == 0) {
                display_quirk_flags_buffer[charBufIndex] |= QUIRK_FLAG_COMBINING_FULL_STOP;
                incrementCharBufIndex = 1;
            } else if (display_text_buffer[textBufIndex - 1] == '.') {
                display_quirk_flags_buffer[charBufIndex] |= QUIRK_FLAG_COMBINING_FULL_STOP;
                incrementCharBufIndex = 1;
            } else {
                display_quirk_flags_buffer[charBufIndex - 1] |= QUIRK_FLAG_COMBINING_FULL_STOP;
            }
            characterHandlingCompleted = 1;
        }
        #endif

        if (display_text_buffer[textBufIndex] == '\n') {
            // If a line break is encountered, skip to the beginning of the next line
            while (charBufCol < DISPLAY_FRAME_WIDTH_CHAR) {
                charBufCol++;
                charBufIndex++;
                if (charBufIndex >= charBufSize) return;
            }
            charBufCol = 0;
            charBufRow++;
            characterHandlingCompleted = 1;
        }
        
        // We do not return earlier because even if the char buffer is full,
        // a combining full stop might come afterwards, which would still fit
        if (charBufIndex >= charBufSize) return;

        if (!characterHandlingCompleted) {
            // If none of the above cases were true, treat the character as a normal character
            display_char_buffer[charBufIndex] = display_text_buffer[textBufIndex];
            incrementCharBufIndex = 1;
        }

        if (incrementCharBufIndex) {
            // Increase the char buffer index and positions if a character was added
            charBufIndex++;
            charBufCol++;
            if (charBufCol >= DISPLAY_FRAME_WIDTH_CHAR) {
                charBufCol = 0;
                charBufRow++;
            }
        }
    }
}

char* buffer_escape_string(char* input, char* charsToEscape, char escapePrefix, uint16_t numEscapeChars) {
    // First pass: Count characters to escape
    size_t escapeCharCount = 0;
    size_t inputLen = strlen(input);
    for (size_t i = 0; i < inputLen; i++) {
        for (uint16_t j = 0; j < numEscapeChars; j++) {
            if (input[i] == charsToEscape[j]) {
                escapeCharCount++;
                break;
            }
        }
    }

    // Second pass: Allocate buffer for output
    size_t outputLen = inputLen + escapeCharCount;
    char* output = calloc(outputLen + 1, 1);
    size_t k = 0;
    for (size_t i = 0; i < inputLen; i++) {
        for (uint16_t j = 0; j < numEscapeChars; j++) {
            if (input[i] == charsToEscape[j]) {
                output[k++] = escapePrefix;
                break;
            }
        }
        output[k++] = input[i];
    }
    return output;
}
#endif

esp_err_t buffer_from_string(const char* in_buf_str, uint8_t is_base64, uint8_t* out_buf, size_t out_buf_size, const char* log_tag) {
    size_t b64_len = 0;
    size_t buffer_str_len = strlen(in_buf_str);
    if (is_base64) {
        unsigned char* buffer_str_uchar = (unsigned char*)in_buf_str;
        int result = mbedtls_base64_decode(NULL, 0, &b64_len, buffer_str_uchar, buffer_str_len);
        if (result == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
            // We don't cover MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL here
            // because this will always be returned when checking size
            ESP_LOGE(log_tag, "MBEDTLS_ERR_BASE64_INVALID_CHARACTER in buffer");
            return ESP_FAIL;
        } else {
            b64_len = 0;
            result = mbedtls_base64_decode(out_buf, out_buf_size, &b64_len, buffer_str_uchar, buffer_str_len);
            if (result != 0) {
                ESP_LOGE(log_tag, "mbedtls_base64_decode() failed for buffer");
                return ESP_FAIL;
            }
        }
    } else {
        memcpy(out_buf, in_buf_str, MIN(buffer_str_len, out_buf_size));
    }
    return ESP_OK;
}

esp_err_t buffer_to_base64(uint8_t* buf, size_t buf_size, uint8_t** out) {
    if (buf != NULL) {
        size_t b64_len = 0;
        mbedtls_base64_encode(NULL, 0, &b64_len, buf, buf_size);
        size_t b64_bufsize = b64_len;
        *out = malloc(b64_len);
        b64_len = 0;
        mbedtls_base64_encode(*out, b64_bufsize, &b64_len, buf, buf_size);
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}
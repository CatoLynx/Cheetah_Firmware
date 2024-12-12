#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef enum {
    MT_OVERWRITE,
    MT_KEEP_1,
    MT_KEEP_0
} buf_merge_t;

typedef enum {
    QUIRK_FLAG_COMBINING_FULL_STOP = (1 << 0),
} quirk_flag_t;


void buffer_8to1(uint8_t* buf8, uint8_t* buf1, uint16_t width, uint16_t height, buf_merge_t mergeType);
void buffer_utf8_to_iso88591(char* dst, char* src);
void buffer_iso88591_to_utf8(char* dst, char* src);
void buffer_textbuf_to_charbuf(uint8_t* display_text_buffer, uint8_t* display_char_buffer, uint16_t* display_quirk_flags_buffer, uint16_t textBufSize, uint16_t charBufSize);
char* buffer_escape_string(char* input, char* charsToEscape, char escapePrefix, uint16_t numEscapeChars);
esp_err_t buffer_from_string(const char* in_buf_str, uint8_t is_base64, uint8_t* out_buf, size_t out_buf_size, const char* log_tag);
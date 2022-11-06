#include "util_generic.h"
#include <ctype.h>
#include <string.h>

uint8_t count_set_bits(uint8_t byte) {
  static const uint8_t NIBBLE_LOOKUP[16] =  {
    0, 1, 1, 2, 1, 2, 2, 3, 
    1, 2, 2, 3, 2, 3, 3, 4
  };
  return NIBBLE_LOOKUP[byte & 0x0F] + NIBBLE_LOOKUP[byte >> 4];
}

uint8_t int_num_digits(int64_t n, uint8_t includeNegSign) {
  // Returns the number of digits in an integer.
  // Includes the negative sign as a digit if includeNegSign is 1.
  uint8_t result = 1;
  if (includeNegSign && n < 0) result++;
  if (n < 0) n = (n == I64_MIN) ? I64_MAX: -n;
  while (n > 9) {
      n /= 10;
      result++;
  }
  return result;
}

uint8_t uint_num_digits(uint64_t n) {
  // Returns the number of digits in an integer.
  uint8_t result = 1;
  while (n > 9) {
      n /= 10;
      result++;
  }
  return result;
}

void str_toUpper(char* str) {
  while (*str) {
    *str = toupper((unsigned char) *str);
    str++;
  }
}

void str_filterAllowed(char* out, char* in, char* allowedChars, bool allowLineBreaks) {
  char* allowedCharsOrig = allowedChars;
  while (*in) {
    allowedChars = allowedCharsOrig;
    if (allowLineBreaks && *in == '\n') {
      *out = *in;
      out++;
    } else {
      while (*allowedChars) {
        if (*allowedChars == *in) {
          *out = *in;
          out++;
          break;
        }
        allowedChars++;
      }
    }
    in++;
  }
}

void str_filterDisallowed(char* out, char* in, char* disallowedChars, bool allowLineBreaks) {
  uint8_t match = 0;
  char* disallowedCharsOrig = disallowedChars;
  while (*in) {
    disallowedChars = disallowedCharsOrig;
    if (allowLineBreaks && *in == '\n') {
      *out = *in;
      out++;
    } else {
      while (*disallowedChars) {
        if (*disallowedChars == *in) {
          match = 1;
          break;
        }
        disallowedChars++;
      }
      if (!match) {
        *out = *in;
        out++;
      }
    }
    in++;
  }
}

void str_filterRangeAllowed(char* out, char* in, uint8_t rangeMin, uint8_t rangeMax, bool allowLineBreaks) {
  while (*in) {
    if ((allowLineBreaks && *in == '\n') || (*in >= rangeMin && *in <= rangeMax)) {
      *out = *in;
      out++;
    }
    in++;
  }
}

void str_filterRangeDisallowed(char* out, char* in, uint8_t rangeMin, uint8_t rangeMax, bool allowLineBreaks) {
  while (*in) {
    if ((allowLineBreaks && *in == '\n') || !(*in >= rangeMin && *in <= rangeMax)) {
      *out = *in;
      out++;
    }
    in++;
  }
}

void str_convertLineBreaks(char* out, char* in, uint16_t numLines, uint16_t charsPerLine) {
  uint16_t curLine = 0;
  uint16_t curCol = 0;
  uint16_t charCount = 0;
  uint16_t maxChars = numLines * charsPerLine;
  while (*in && charCount < maxChars) {
    if (*in == '\n') {
      while (curCol < charsPerLine) {
        *out = ' ';
        out++;
        charCount++;
        curCol++;
      }
      curLine++;
      curCol = 0;
    } else {
      *out = *in;
      out++;
      charCount++;
      curCol++;
      if (curCol >= charsPerLine) {
        curLine++;
        curCol = 0;
      }
    }
    in++;
  }
}

void str_insertLineBreaks(char* out, char* in, uint16_t interval, size_t inLen) {
  uint16_t i = 0;
  while (*in && i < inLen) {
    for (uint16_t i = 0; i < interval; i++) {
      if (!*in) break;
      *out = *in;
      out++;
      in++;
      i++;
    }
    *out = '\n';
    out++;
  }
}
#include "util_generic.h"
#include <ctype.h>
#include <string.h>
#include <sys/time.h>
#include "esp_log.h"

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
  uint16_t inPos = 0;
  while (*in && inPos < inLen) {
    for (uint16_t n = 0; n < interval; n++) {
      if (!*in) break;
      *out = *in;
      out++;
      in++;
      inPos++;
    }
    if (inPos < inLen) {
        *out = '\n';
        out++;
    }
  }
}

// Copied from https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
color_hsv_t rgb2hsv(color_rgb_t in) {
    color_hsv_t         out;
    double      min, max, delta;

    min = in.r < in.g ? in.r : in.g;
    min = min  < in.b ? min  : in.b;

    max = in.r > in.g ? in.r : in.g;
    max = max  > in.b ? max  : in.b;

    out.v = max;                                // v
    delta = max - min;
    if (delta < 0.00001)
    {
        out.s = 0;
        out.h = 0; // undefined, maybe nan?
        return out;
    }
    if( max > 0.0 ) { // NOTE: if Max is == 0, this divide would cause a crash
        out.s = (delta / max);                  // s
    } else {
        // if max is 0, then r = g = b = 0              
        // s = 0, h is undefined (just set it to 0 tho)
        out.s = 0.0;
        out.h = 0.0;
        return out;
    }
    if( in.r >= max )                           // > is bogus, just keeps compilor happy
        out.h = ( in.g - in.b ) / delta;        // between yellow & magenta
    else
    if( in.g >= max )
        out.h = 2.0 + ( in.b - in.r ) / delta;  // between cyan & yellow
    else
        out.h = 4.0 + ( in.r - in.g ) / delta;  // between magenta & cyan

    out.h *= 60.0;                              // degrees

    if( out.h < 0.0 )
        out.h += 360.0;

    return out;
}

// Copied from https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
color_rgb_t hsv2rgb(color_hsv_t in) {
    double      hh, p, q, t, ff;
    long        i;
    color_rgb_t         out;

    if(in.s <= 0.0) {       // < is bogus, just shuts up warnings
        out.r = in.v;
        out.g = in.v;
        out.b = in.v;
        return out;
    }
    hh = in.h;
    if(hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));

    switch(i) {
    case 0:
        out.r = in.v;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = in.v;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = in.v;
        out.b = t;
        break;

    case 3:
        out.r = p;
        out.g = q;
        out.b = in.v;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = in.v;
        break;
    case 5:
    default:
        out.r = in.v;
        out.g = p;
        out.b = q;
        break;
    }
    return out;     
}

int64_t time_getSystemTime_us() {
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  return (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
}

int32_t map_int32(int32_t in, int32_t inMin, int32_t inMax, int32_t outMin, int32_t outMax) {
  return (in - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

double map_double(double in, double inMin, double inMax, double outMin, double outMax) {
  return (in - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}
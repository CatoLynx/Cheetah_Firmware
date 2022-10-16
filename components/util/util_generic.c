#include "util_generic.h"

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
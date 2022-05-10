#include "util_generic.h"

uint8_t count_set_bits(uint8_t byte) {
  static const uint8_t NIBBLE_LOOKUP[16] =  {
    0, 1, 1, 2, 1, 2, 2, 3, 
    1, 2, 2, 3, 2, 3, 3, 4
  };
  return NIBBLE_LOOKUP[byte & 0x0F] + NIBBLE_LOOKUP[byte >> 4];
}
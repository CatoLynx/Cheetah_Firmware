#include <stdint.h>

const uint8_t char_seg_font_min = 32;
const uint8_t char_seg_font_max = 127;

/*

  SEGMENT MAPPING

  Seg | Bit
  a   | 1000000000000000
  b   | 0000000000000001
  c   | 0000000000000100
  d   | 0000000000100000
  e   | 0000000001000000
  f   | 0000000010000000
  g   | 0000000100000000
  h   | 0001000000000000
  k   | 0010000000000000
  m   | 0100000000000000
  n   | 0000000000000010
  p   | 0000000000001000
  r   | 0000000000010000
  s   | 0000010000000000
  t   | 0000001000000000
  u   | 0000100000000000

*/

typedef enum SEGMENTS {
  A = 32768,
  B = 1,
  C = 4,
  D = 32,
  E = 64,
  F = 128,
  G = 256,
  H = 4096,
  K = 8192,
  M = 16384,
  N = 2,
  P = 8,
  R = 16,
  S = 1024,
  T = 512,
  U = 2048,
  DP = 65536
} seg_t;

// Starts at dec 32 (Space)
const uint32_t char_16seg_font[96] = {
  /*   */ 0,
  /* ! */ A | B | E | F | K | M | N,
  /* " */ C | H,
  /* # */ C | D | E | F | M | P | S | U,
  /* $ */ A | B | D | E | F | H | M | P | S | U,
  /* % */ A | E | N | T,
  /* & */ A | E | F | G | H | M | P | R | U,
  /* ' */ M,
  /* ( */ A | F | G | H,
  /* ) */ B | C | D | E,
  /* * */ K | M | N | P | R | S | T | U,
  /* + */ M | P | S | U,
  /* , */ T,
  /* - */ P | U,
  /* . */ DP,
  /* / */ N | T,
  /* 0 */ A | B | C | D | E | F | G | H | N | T,
  /* 1 */ C | D | N,
  /* 2 */ A | B | C | E | F | G | P | U,
  /* 3 */ A | B | C | D | E | F | P | U,
  /* 4 */ C | D | H | P | U,
  /* 5 */ A | B | D | E | F | H | P | U,
  /* 6 */ A | B | D | E | F | G | H | P | U,
  /* 7 */ A | B | C | D,
  /* 8 */ A | B | C | D | E | F | G | H | P | U,
  /* 9 */ A | B | C | D | E | F | H | P | U,
  /* : */ F | U,
  /* ; */ A | T,
  /* < */ N | R,
  /* = */ E | F | P | U,
  /* > */ K | T,
  /* ? */ A | B | C | P | S,
  /* @ */ A | B | C | D | E | G | H | P | S,
  /* A */ A | B | C | D | G | H | P | U,
  /* B */ A | B | C | D | E | F | M | P | S,
  /* C */ A | B | E | F | G | H,
  /* D */ A | B | C | D | E | F | M | S,
  /* E */ A | B | E | F | G | H | U,
  /* F */ A | B | G | H | U,
  /* G */ A | B | D | E | F | G | H | P,
  /* H */ C | D | G | H | P | U,
  /* I */ A | B | E | F | M | S,
  /* J */ C | D | E | F,
  /* K */ G | H | N | R | U,
  /* L */ E | F | G | H,
  /* M */ C | D | G | H | K | N,
  /* N */ C | D | G | H | K | R,
  /* O */ A | B | C | D | E | F | G | H,
  /* P */ A | B | C | G | H | P | U,
  /* Q */ A | B | C | D | E | F | G | H | R,
  /* R */ A | B | C | G | H | P | R | U,
  /* S */ A | B | D | E | F | H | P | U,
  /* T */ A | B | M | S,
  /* U */ C | D | E | F | G | H,
  /* V */ G | H | N | T,
  /* W */ C | D | G | H | R | T,
  /* X */ K | N | R | T,
  /* Y */ K | N | S,
  /* Z */ A | B | E | F | N | T,
  /* [ */ B | E | M | S,
  /* \ */ K | R,
  /* ] */ A | F | M | S,
  /* ^ */ H | K,
  /* _ */ E | F,
  /* ` */ K,
  /* a */ E | F | G | S | U,
  /* b */ D | E | F | G | H | P | U,
  /* c */ E | F | G | P | U,
  /* d */ C | D | E | F | G | P | U,
  /* e */ A | B | C | E | F | G | H | P | U,
  /* f */ B | M | P | S | U,
  /* g */ B | C | D | E | M | P,
  /* h */ D | G | H | P | U,
  /* i */ A | B | S,
  /* j */ A | B | F | S,
  /* k */ G | H | N | R | U,
  /* l */ G | H,
  /* m */ D | G | P | S | U,
  /* n */ D | G | P | U,
  /* o */ D | E | F | G | P | U,
  /* p */ A | G | H | M | U,
  /* q */ B | C | D | M | P,
  /* r */ G | U,
  /* s */ E | P | R,
  /* t */ F | G | H | U,
  /* u */ D | E | F | G,
  /* v */ G | T,
  /* w */ D | G | R | T,
  /* x */ K | N | R | T,
  /* y */ F | K | N | S,
  /* z */ F | T | U,
  /* { */ N | R | U,
  /* | */ M | S,
  /* } */ K | P | T,
  /* ~ */ P | U,
  /* DEL */ A | B | C | D | E | F | G | H | K | M | N | P | R | S | T | U | DP
};
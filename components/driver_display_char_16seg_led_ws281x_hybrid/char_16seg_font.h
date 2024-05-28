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
  A1 = 32768,
  A2 = 1,
  B = 4,
  C = 32,
  D2 = 64,
  D1 = 128,
  E = 256,
  F = 4096,
  J = 8192,
  H = 16384,
  K = 2,
  G2 = 8,
  L = 16,
  I = 1024,
  M = 512,
  G1 = 2048,
  DP = 65536
} seg_t;

// Starts at dec 32 (Space)
const uint32_t char_16seg_font[96] = {
  /*   */ 0,
  /* ! */ A1 | A2 | D2 | D1 | J | H | K,
  /* " */ B | F,
  /* # */ B | C | D2 | D1 | H | G2 | I | G1,
  /* $ */ A1 | A2 | C | D2 | D1 | F | H | G2 | I | G1,
  /* % */ A1 | D2 | K | M,
  /* & */ A1 | D2 | D1 | E | F | H | G2 | L | G1,
  /* ' */ H,
  /* ( */ A1 | D1 | E | F,
  /* ) */ A2 | B | C | D2,
  /* * */ J | H | K | G2 | L | I | M | G1,
  /* + */ H | G2 | I | G1,
  /* , */ M,
  /* - */ G2 | G1,
  /* . */ DP,
  /* / */ K | M,
  /* 0 */ A1 | A2 | B | C | D2 | D1 | E | F | K | M,
  /* 1 */ B | C | K,
  /* 2 */ A1 | A2 | B | D2 | D1 | E | G2 | G1,
  /* 3 */ A1 | A2 | B | C | D2 | D1 | G2 | G1,
  /* 4 */ B | C | F | G2 | G1,
  /* 5 */ A1 | A2 | C | D2 | D1 | F | G2 | G1,
  /* 6 */ A1 | A2 | C | D2 | D1 | E | F | G2 | G1,
  /* 7 */ A1 | A2 | B | C,
  /* 8 */ A1 | A2 | B | C | D2 | D1 | E | F | G2 | G1,
  /* 9 */ A1 | A2 | B | C | D2 | D1 | F | G2 | G1,
  /* : */ D1 | G1,
  /* ; */ A1 | M,
  /* < */ K | L,
  /* = */ D2 | D1 | G2 | G1,
  /* > */ J | M,
  /* ? */ A1 | A2 | B | G2 | I,
  /* @ */ A1 | A2 | B | C | D2 | E | F | G2 | I,
  /* A */ A1 | A2 | B | C | E | F | G2 | G1,
  /* B */ A1 | A2 | B | C | D2 | D1 | H | G2 | I,
  /* C */ A1 | A2 | D2 | D1 | E | F,
  /* D */ A1 | A2 | B | C | D2 | D1 | H | I,
  /* E */ A1 | A2 | D2 | D1 | E | F | G1,
  /* F */ A1 | A2 | E | F | G1,
  /* G */ A1 | A2 | C | D2 | D1 | E | F | G2,
  /* H */ B | C | E | F | G2 | G1,
  /* I */ A1 | A2 | D2 | D1 | H | I,
  /* J */ B | C | D2 | D1,
  /* K */ E | F | K | L | G1,
  /* L */ D2 | D1 | E | F,
  /* M */ B | C | E | F | J | K,
  /* N */ B | C | E | F | J | L,
  /* O */ A1 | A2 | B | C | D2 | D1 | E | F,
  /* P */ A1 | A2 | B | E | F | G2 | G1,
  /* Q */ A1 | A2 | B | C | D2 | D1 | E | F | L,
  /* R */ A1 | A2 | B | E | F | G2 | L | G1,
  /* S */ A1 | A2 | C | D2 | D1 | F | G2 | G1,
  /* T */ A1 | A2 | H | I,
  /* U */ B | C | D2 | D1 | E | F,
  /* V */ E | F | K | M,
  /* W */ B | C | E | F | L | M,
  /* X */ J | K | L | M,
  /* Y */ J | K | I,
  /* Z */ A1 | A2 | D2 | D1 | K | M,
  /* [ */ A2 | D2 | H | I,
  /* \ */ J | L,
  /* ] */ A1 | D1 | H | I,
  /* ^ */ F | J,
  /* _ */ D2 | D1,
  /* ` */ J,
  /* a */ D2 | D1 | E | I | G1,
  /* b */ C | D2 | D1 | E | F | G2 | G1,
  /* c */ D2 | D1 | E | G2 | G1,
  /* d */ B | C | D2 | D1 | E | G2 | G1,
  /* e */ A1 | A2 | B | D2 | D1 | E | F | G2 | G1,
  /* f */ A2 | H | G2 | I | G1,
  /* g */ A2 | B | C | D2 | H | G2,
  /* h */ C | E | F | G2 | G1,
  /* i */ A1 | A2 | I,
  /* j */ A1 | A2 | D1 | I,
  /* k */ E | F | K | L | G1,
  /* l */ E | F,
  /* m */ C | E | G2 | I | G1,
  /* n */ C | E | G2 | G1,
  /* o */ C | D2 | D1 | E | G2 | G1,
  /* p */ A1 | E | F | H | G1,
  /* q */ A2 | B | C | H | G2,
  /* r */ E | G1,
  /* s */ D2 | G2 | L,
  /* t */ D1 | E | F | G1,
  /* u */ C | D2 | D1 | E,
  /* v */ E | M,
  /* w */ C | E | L | M,
  /* x */ J | K | L | M,
  /* y */ D1 | J | K | I,
  /* z */ D1 | M | G1,
  /* { */ K | L | G1,
  /* | */ H | I,
  /* } */ J | G2 | M,
  /* ~ */ G2 | G1,
  /* DEL */ A1 | A2 | B | C | D2 | D1 | E | F | J | H | K | G2 | L | I | M | G1 | DP
};
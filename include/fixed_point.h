#pragma once

#include <stdint.h>

typedef int32_t fx20_12_t;
typedef int64_t fx52_12_t;

#define FX20_12(num) (((fx20_12_t)num) << 12)
#define UNFX20_12(num) ((num) >> 12)
#define UNFX20_12_ROUND(num) (UNFX20_12(num + (1 << 11)))

#define FX52_12(num) (((fx52_12_t)num) << 12)
#define UNFX52_12(num) ((num) >> 12)
#define UNFX52_12_ROUND(num) (UNFX52_12(num + (1 << 11)))
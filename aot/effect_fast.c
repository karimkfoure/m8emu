/* The effect block has a separate translation unit so the compiler can
 * keep its state local without expanding the large translated dispatch
 * bodies. */
#define M8_AOT_EFFECT_FAST_IMPLEMENTATION 1
#include "effect_fast.h"

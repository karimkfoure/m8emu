/* The HYPERSYNTH loop has a separate translation unit so the compiler can
 * keep its state local without expanding the large translated dispatch
 * bodies. */
#define M8_AOT_HYPER_FAST_IMPLEMENTATION 1
#include "hyper_fast.h"

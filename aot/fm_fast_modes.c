/* Separate mixed-operator loop preserves the mode-0 compiler layout. */
#define M8_AOT_FM_STRUCTURAL 1
#define M8_AOT_FM_DYNAMIC_BIAS 1
#define M8_AOT_FM_MODE8 1
#define M8_AOT_FM_FAST_MODES 1
#define M8_AOT_FM_FAST_IMPLEMENTATION 1
#include "fm_batch.h"
int m8_fm_fast_modes_kernel(m8_cpu *cpu,m8_mem *mem) {
#if defined(M8_AOT_FM_FAST_BATCH_H)
    return m8_fms_batch_kernel(cpu,mem);
#else
    (void)cpu;(void)mem;return 0;
#endif
}

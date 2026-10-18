/* The FM loop has a separate translation unit so the compiler can keep
 * its state local without expanding the large translated dispatch bodies. */
#define M8_AOT_FM_STRUCTURAL 1
#define M8_AOT_FM_DYNAMIC_BIAS 1
#define M8_AOT_FM_FAST_IMPLEMENTATION 1
#include "fm_batch.h"

int m8_fm_fast_modes_kernel(m8_cpu *,m8_mem *);

int m8_fm_fast_kernel(m8_cpu *cpu,m8_mem *mem) {
#if defined(M8_AOT_FM_FAST_BATCH_H)
    /* The original bounded loop is cheaper for its unit-modulation shape.
     * Select it with a short layout test, so general voices do not repeat
     * the original structural preflight before the new one. */
    if(mem->dtcm.base==0x20000000u && mem->dtcm.size<=0x200000u &&
       mem->dtcm.host && (uintptr_t)mem->dtcm.host<=UINTPTR_MAX-mem->dtcm.size) {
        uint8_t *object=m8_region_pointer(&mem->dtcm,cpu->r[8],0x254);
        if(object && (object[0x8e] || object[0xde] || object[0x12e] || object[0x17e]))
            return m8_fm_fast_modes_kernel(cpu,mem);
        if(object && object[0x40]==6 && object[0x41]<=3) {
            uint32_t address=m8_le_read(object+0x20c,4);
            uint8_t *modulation=m8_region_pointer(&mem->dtcm,address,4);
            if(modulation && m8_le_read(modulation,4)==0x3f800000u) {
                unsigned i=1;
                while(i<16 && m8_le_read(object+0x20c+4*i,4)==address)i++;
                if(i==16 && m8_fm_batch_bounded_kernel(cpu,mem))return 1;
            }
        }
    }
    return m8_fms_batch_kernel(cpu,mem);
#else
    /* Diagnostic and scalar-only builds use the translated body. */
    (void)cpu;(void)mem;return 0;
#endif
}

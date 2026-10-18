#ifndef M8RT_NATIVE_DEVICE_FAST_H
#define M8RT_NATIVE_DEVICE_FAST_H

/* Include after the native mask enum, should_stop() and private MMIO state.
 * The original callback owns each rejected access. This helper does not cache
 * pending interrupts, device values or the SAI clock. */
#ifndef M8RT_NATIVE_DEVICE_FAST
#define M8RT_NATIVE_DEVICE_FAST 0
#endif

#include "native-sai-quiet.h"

static inline bool m8rt_native_device_fast_quiet(m8rt *rt) {
    return m8rt_native_sai_quiet(rt);
}

#if M8RT_NATIVE_DEVICE_FAST
static inline __attribute__((always_inline)) bool
m8rt_native_device_fast_admit(m8rt *rt,uint32_t addr,uint32_t size) {
    /* ISER and ICER differ only in bit 7; bank bits are 4:2. All other
     * address bits, including alignment, must equal the ISER base. */
    return size==4 && (addr&~UINT32_C(0x9c))==UINT32_C(0xe000e100) &&
        rt->native.active && rt->native.device_direct &&
        !m8rt_native_boot_copying(rt) && !rt->native.boot_callback_irq && !rt->audit;
}

static inline __attribute__((always_inline)) bool
m8rt_native_device_fast_masks(m8rt *rt,uint32_t *cpsr) {
    if(!rt->aot_active || !(rt->aot_services&M8_AOT_SERVICE_NVIC)) return false;
    if(rt->aot_mask_cache==AOT_MASK_SAFE) {
        *cpsr=rt->aot_mask_cpsr;
        return true;
    }
    if(rt->aot_mask_cache==AOT_MASK_UNSAFE) return false;
    /* Preserve the native register API's CPSR merge and mask-cache effects. */
    uint32_t current=(rt->native.cpsr&~UINT32_C(0xfe0ffc00)) |
        (rt->native.cpu.xpsr&UINT32_C(0xfe0ffc00));
    bool safe=!rt->native.arch.primask && !rt->native.arch.basepri;
    if(rt->aot_mask_cache==AOT_MASK_UNREAD) {
        rt->aot_mask_cache=safe?AOT_MASK_SAFE:AOT_MASK_UNSAFE;
        if(safe) rt->aot_mask_cpsr=current;
    }
    if(!safe) return false;
    *cpsr=current;
    return true;
}

static inline __attribute__((always_inline)) int
m8rt_native_device_fast_finish(m8rt *rt,bool admitted,uint32_t cpsr) {
    int result;
    if(rt->fatal) result=M8_AOT_FAULT;
    else if(!admitted || rt->aot_sdhc_pending || should_stop(rt)) result=0;
    else {
        result=M8_AOT_MEM_CONTINUE;
        if(!rt->irq_nest && !(cpsr&0x80u)) {
            if(rt->audio_pending ||
               (rt->usb_pending && (rt->nvic_iser[113u>>5]&(1u<<(113u&31u)))) ||
               (rt->systick_pending && (rt->syst_csr&3u)==3u)) result=0;
            else if(rt->nvic_pending_banks) for(unsigned bank=0;bank<8;bank++) {
                if(rt->nvic_iser[bank]&rt->nvic_ispr[bank]) { result=0;break; }
            }
        }
    }
    /* NVIC has no service token, live-storage effect or DWT exception. */
    if(rt->native.chain && result!=M8_AOT_MEM_CONTINUE) rt->native.service_exit=true;
    return result;
}

static inline __attribute__((always_inline)) int
m8rt_native_device_fast_read(m8rt *rt,uint32_t addr,uint32_t size,
                             uint32_t *value,int *result) {
    /* A runtime output alias can change the final active/boot/IRQ checks.
     * Keep it with the original callback. Prove quiet SAI before any effects
     * so a rejection can run the original owner once. */
    if(!m8rt_native_device_fast_admit(rt,addr,size) ||
       (uintptr_t)value-(uintptr_t)rt<sizeof(*rt) ||
       !m8rt_native_device_fast_quiet(rt)) return 0;
    uint32_t cpsr=0;
    bool admitted=m8rt_native_device_fast_masks(rt,&cpsr);
    *value=!(addr&UINT32_C(0x80))?
        rt->nvic_iser[(addr&31u)/4u]:rt->mmio->scs[(addr-0xe000e000u)/4u];
    *result=m8rt_native_device_fast_finish(rt,admitted,cpsr);
    return 1;
}

static inline __attribute__((always_inline)) int
m8rt_native_device_fast_write(m8rt *rt,uint32_t addr,uint32_t size,
                              uint32_t value,int *result) {
    if(!m8rt_native_device_fast_admit(rt,addr,size)) return 0;
    uint32_t cpsr=0;
    bool admitted=m8rt_native_device_fast_masks(rt,&cpsr);
    unsigned bank=(addr&31u)/4u;
    if(!(addr&UINT32_C(0x80))) rt->nvic_iser[bank]|=value;
    else rt->nvic_iser[bank]&=~value;
    *result=m8rt_native_device_fast_finish(rt,admitted,cpsr);
    return 1;
}
#else
static inline int m8rt_native_device_fast_read(m8rt *rt,uint32_t addr,uint32_t size,
                                              uint32_t *value,int *result) {
    (void)rt;(void)addr;(void)size;(void)value;(void)result;
    return 0;
}
static inline int m8rt_native_device_fast_write(m8rt *rt,uint32_t addr,uint32_t size,
                                               uint32_t value,int *result) {
    (void)rt;(void)addr;(void)size;(void)value;(void)result;
    return 0;
}
#endif
#endif

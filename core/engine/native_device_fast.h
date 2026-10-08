#ifndef M8RT_NATIVE_DEVICE_FAST_H
#define M8RT_NATIVE_DEVICE_FAST_H

/* Include after the native mask enum, should_stop() and private MMIO state.
 * The original callback owns each rejected access. This helper does not cache
 * pending interrupts, device values or the SAI clock. */
#ifndef M8RT_NATIVE_DEVICE_FAST
#define M8RT_NATIVE_DEVICE_FAST 0
#endif

static inline bool m8rt_native_device_fast_quiet(m8rt *rt) {
    m8rt_mmio *m=rt->mmio;
    return !m->periph[0xe800c/4] ||
        (!m->route_dirty && !rt->fatal && m->route>=0 &&
         m->route==m->clock_route && m->clock_last==rt->cyccnt && !m->pending);
}

#if M8RT_NATIVE_DEVICE_FAST
static inline __attribute__((always_inline)) bool
m8rt_native_device_fast_admit(m8rt *rt,uint32_t addr,uint32_t size) {
    return size==4 && !(addr&3u) &&
        ((uint32_t)(addr-UINT32_C(0xe000e100))<=28u ||
         (uint32_t)(addr-UINT32_C(0xe000e180))<=28u) &&
        rt->native.active && rt->native.device_direct &&
        !rt->native.boot && !rt->native.boot_callback_irq && !rt->audit;
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
    /* These profile stages keep the original order, including each bank scan. */
    M8_ACCESS_NVIC(rt,0);
    if(admitted) M8_ACCESS_NVIC(rt,1);
    if(rt->fatal) result=M8_AOT_FAULT;
    else if(!admitted || rt->aot_sdhc_pending || should_stop(rt)) result=0;
    else {
        result=M8_AOT_MEM_CONTINUE;
        if(!rt->irq_nest && !(cpsr&0x80u)) {
            M8_ACCESS_NVIC(rt,2);
            if(rt->audio_pending ||
               (rt->usb_pending && (rt->nvic_iser[113u>>5]&(1u<<(113u&31u)))) ||
               (rt->systick_pending && (rt->syst_csr&3u)==3u)) result=0;
            else if(rt->nvic_pending_banks) for(unsigned bank=0;bank<8;bank++) {
                M8_ACCESS_NVIC(rt,3);
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
    M8_ACCESS_RECORD(rt,addr,size,2,0);
    uint32_t cpsr=0;
    bool admitted=m8rt_native_device_fast_masks(rt,&cpsr);
    *value=(uint32_t)(addr-UINT32_C(0xe000e100))<=28u?
        rt->nvic_iser[(addr&31u)/4u]:rt->mmio->scs[(addr-0xe000e000u)/4u];
    *result=m8rt_native_device_fast_finish(rt,admitted,cpsr);
    return 1;
}

static inline __attribute__((always_inline)) int
m8rt_native_device_fast_write(m8rt *rt,uint32_t addr,uint32_t size,
                              uint32_t value,int *result) {
    if(!m8rt_native_device_fast_admit(rt,addr,size)) return 0;
    M8_ACCESS_RECORD(rt,addr,size,2,1);
    uint32_t cpsr=0;
    bool admitted=m8rt_native_device_fast_masks(rt,&cpsr);
    unsigned bank=(addr&31u)/4u;
    if((uint32_t)(addr-UINT32_C(0xe000e100))<=28u) rt->nvic_iser[bank]|=value;
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

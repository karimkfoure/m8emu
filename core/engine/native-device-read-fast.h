#ifndef M8RT_NATIVE_DEVICE_READ_FAST_H
#define M8RT_NATIVE_DEVICE_READ_FAST_H

/* The caller has installed the native bus. Admit only a word read whose SAI
 * owner would perform no work. The read then cannot change the epoch, clock,
 * IRQ nesting or output queues, so no before/after snapshot is required.
 * Keep both service-observer calls, including their deadline poll counts. */
static inline int m8rt_native_plain_read_fast(m8rt *rt,uint32_t addr,
                                              uint32_t size,uint32_t *value,
                                              int *result) {
    if(size!=4 || (addr&3u) || !native_plain_device_read(addr)) return 0;
    /* A callback output can alias state observed by the original token. Keep
     * that case on the snapshot path before writing any value or counter. */
    if((uintptr_t)value-(uintptr_t)rt<sizeof(*rt)) return 0;
    if(!rt->native.active || !rt->native.region || !rt->native.service_continue ||
       m8rt_native_boot_copying(rt) || rt->native.boot_callback_irq || rt->audit || rt->fatal)
        return 0;
    if(!m8rt_native_device_fast_quiet(rt)) return 0;

    bool admitted=!m8rt_native_service_observer(rt);
    *value=rt->mmio->periph[(addr-UINT32_C(0x40000000))/4];
    /* The ordinary NVIC result is unadmitted for these addresses. Its only
     * counter effect is this stage; no pending-bank scan or stop poll runs. */
    int code=rt->fatal?M8_AOT_FAULT:
        admitted && !m8rt_native_service_observer(rt)?M8_AOT_MEM_CONTINUE:0;
    /* None of the admitted addresses is in the live storage window or DWT.
     * Preserve the post-access chain exit after a declined continuation. */
    if(rt->native.chain && code!=M8_AOT_MEM_CONTINUE) rt->native.service_exit=true;
    *result=code;
    return 1;
}

#endif

#ifndef M8RT_NATIVE_SERVICES_H
#define M8RT_NATIVE_SERVICES_H

/* Private, default-off continuation contract. Device callbacks still run
 * exactly once. A changed schedule or pending observer retains the old exit. */
typedef struct {
    bool admitted;
#ifdef M8RT_NATIVE_ENGINE
    uint64_t epoch,cycles,pcm,local;
    uint32_t clock,pc;
    int nesting;
#endif
} m8rt_native_service_token;

#ifdef M8RT_NATIVE_ENGINE
static inline bool m8rt_native_service_address(uint32_t addr,unsigned size,bool read) {
    if(size!=4 || (addr&3u)) return false;
    if(addr-UINT32_C(0x402e0000)<0x200u)
        return read || addr==UINT32_C(0x402e0144) || addr==UINT32_C(0x402e01bc);
    if(!read) return addr==UINT32_C(0xe000ef70);
    if(addr-UINT32_C(0x402c0000)<0x8000u) {
        switch(addr&0xfffu) {
        case 0x10: case 0x14: case 0x18: case 0x1c: case 0x20:
        case 0x24: case 0x2c: case 0x30: case 0x34: case 0x38: return true;
        default:return false;
        }
    }
    return addr==UINT32_C(0x40384008) || addr==UINT32_C(0x400e800c) ||
        addr==UINT32_C(0x400e8024) || addr==UINT32_C(0x400e802c) ||
        addr-UINT32_C(0x400e9000)<0x400u;
}
static inline bool m8rt_native_service_observer(m8rt *rt) {
    if(rt->deadline_ns && !(rt->native.service_polls++ & 255u) &&
       m8rt_now_ns()>=rt->deadline_ns) {
        rt->native.service_exit=true;
        if(rt->native_live.deadline_yield) rt->native_live.deadline_seen=true;
    }
    return rt->fatal || rt->native.stopped || rt->native.code_invalidated ||
        rt->native.service_exit || rt->watchdog.reset_pending || rt->aot_sdhc_pending ||
        rt->audio_pending || rt->usb_pending || rt->systick_pending ||
        rt->nvic_pending_banks || atomic_load_explicit(&rt->host_stop,memory_order_relaxed);
}
#endif
static inline m8rt_native_service_token m8rt_native_service_begin(m8rt *rt,
                                      uint32_t addr,unsigned size,bool read) {
    m8rt_native_service_token token={0};
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native.active && rt->native.region && rt->native.service_continue &&
       m8rt_native_service_address(addr,size,read) && !m8rt_native_service_observer(rt)) {
        token=(m8rt_native_service_token){.admitted=true,.epoch=rt->native.service_epoch,
            .cycles=rt->guest_cycles,.pcm=rt->pcm_total,.local=rt->local_total,
            .clock=rt->cyccnt,.pc=rt->native.cpu.r[15],.nesting=rt->irq_nest};
    }
#else
    (void)rt;(void)addr;(void)size;(void)read;
#endif
    return token;
}
static inline bool m8rt_native_service_unchanged(m8rt *rt,
                                               const m8rt_native_service_token *token) {
#ifdef M8RT_NATIVE_ENGINE
    return token->admitted && !m8rt_native_service_observer(rt) &&
        token->epoch==rt->native.service_epoch && token->cycles==rt->guest_cycles &&
        token->clock==rt->cyccnt && token->pcm==rt->pcm_total && token->local==rt->local_total &&
        token->pc==rt->native.cpu.r[15] && token->nesting==rt->irq_nest;
#else
    (void)rt;(void)token;return false;
#endif
}
#endif

#include "internal.h"
#include "thumb-interpreter.h"
#include "thumb-services.h"
#include "native-interpreter-profile.h"
#ifdef M8RT_STATIC_COVERAGE
#include "native_static.h"
#endif

static int fetch(void *context,uint32_t pc,unsigned size,uint8_t *bytes) {
    m8rt *rt=context;
#ifdef M8RT_PURE_NATIVE
    int error=m8_cpu_backend_fetch(rt->uc,pc,bytes,size);
    if(error) rt->native.interpreter_error=error;
    return error;
#else
    /* The reference machine maps only ITCM, flash and boot ROM executable. */
    bool executable=(pc<512u*1024u && size<=512u*1024u-pc) ||
        (pc>=0x60000000u && pc-0x60000000u<8u*1024u*1024u && size<=8u*1024u*1024u-(pc-0x60000000u)) ||
        (pc>=0x00200000u && pc-0x00200000u<128u*1024u && size<=128u*1024u-(pc-0x00200000u));
    return executable?m8rt_mem_read(rt,pc,bytes,size):UC_ERR_FETCH_PROT;
#endif
}
static int instruction_fetch(void *context,uint32_t pc,unsigned size,uint8_t *bytes) {
    m8rt *rt=context;
    return rt->native.code_cache.active?
        m8_thumb_code_cache_fetch(&rt->native.code_cache,pc,size,bytes):
        fetch(rt,pc,size,bytes);
}
#ifdef M8RT_PURE_NATIVE
int m8rt_native_code_cache_begin(m8rt *rt,uint32_t pc,unsigned size,const uint8_t *expected) {
    return m8_thumb_code_cache_begin(&rt->native.code_cache,pc,size,expected,fetch,rt);
}
int m8rt_native_code_store_prepare(m8rt *rt,uint32_t addr,size_t size) {
    if(size && addr<0x60800000u && (uint64_t)addr+size>0x60000000u &&
       (addr<0x607c0000u || (uint64_t)addr+size>0x607ff000u))rt->session_immutable_changed=true;
    if(rt->native.code_cache.active) return UC_ERR_OK;
    uint32_t pc=rt->native.cpu.r[15];
    uint64_t page=pc&~UINT32_C(1023),end=(uint64_t)addr+size;
    /* A pinned M7 block stays in its fetch page, except one wide instruction
     * at its final halfword. Keep the conservative two-byte extension. */
    if(end<=UINT64_C(0x100000000) &&
       (end<=page || (uint64_t)addr>=page+1026u)) return UC_ERR_OK;
    m8rt_fail(rt,"native code store predicate=trusted_block pc=%08x address=%08x size=%zu control=%08x ipsr=%u it=%02x; overlapping active-block writes require a pinned byte snapshot",
        pc,addr,size,rt->native.arch.control,rt->native.arch.ipsr,rt->native.arch.itstate);
    return UC_ERR_WRITE_PROT;
}
#endif
int m8rt_native_step_dispatch(m8_cpu *cpu,m8_mem *mem,m8_native_arch *arch) {
    m8rt *rt=mem->ctx;
    int result=rt->native.interpret_only?2:rt->native.step(cpu,mem,arch);
#ifdef M8RT_STATIC_COVERAGE
#ifndef M8RT_STATIC_REGIONS_ONLY
    /* See native-engine.c: the web cold-only companion is inactive once the
     * warm single-step exists, so warm fallbacks skip the declined attempt. */
    bool static_cold=true;
#ifdef M8RT_STATIC_COLD_ONLY
    static_cold=rt->native.warm_step==NULL;
#endif
    if(static_cold && result==2 && !rt->native.interpret_only)
        result=m8_native_static_step(cpu,mem,arch);
#endif /* !M8RT_STATIC_REGIONS_ONLY */
#endif /* M8RT_STATIC_COVERAGE */
    /* The warm single-step covers playback PCs declined by cold and static
     * coverage. It runs after them so overlapping PCs keep their current
     * compiled path; only doubly-declined PCs change owner, from the
     * portable interpreter to warm compiled code. */
    if(result==2 && !rt->native.interpret_only && rt->native.warm_step)
        result=rt->native.warm_step(cpu,mem,arch);
    if(result!=2) return result;
    rt->native.interpreting=true;
    rt->native.interpreter_error=0;
    result=m8_thumb_interpret_fetch(cpu,mem,arch,instruction_fetch,rt);
    if(rt->native_boot_app.bus_pending || rt->native.boot_callback_irq) {
        m8_native_interpreter_profile_record(rt->interpreter_profile,
            m8_thumb_last_result(),0,arch->last_executed);
        rt->native.portable_guest+=arch->last_executed;
        m8_thumb_code_cache_invalidate(&rt->native.code_cache);
        rt->native.interpreting=false;
        return result; /* The existing callback owner resumes this instruction. */
    }
    m8_thumb_result detail=*m8_thumb_last_result();
    unsigned boundary=m8_thumb_last_boundary();
    bool prepare=detail.reason==M8_THUMB_REASON_FP_CONTEXT;
    int saved_error=rt->native.interpreter_error;
    int error=m8rt_native_interpreter_service(rt,result);
    if(error && saved_error) error=saved_error;
    if(prepare && !error) {
        m8_native_interpreter_profile_record(rt->interpreter_profile,&detail,0,0);
        rt->native.interpreter_error=0;
        result=m8_thumb_interpret_prepared(cpu,mem,arch,instruction_fetch,rt);
        detail=*m8_thumb_last_result();boundary=m8_thumb_last_boundary();
        saved_error=rt->native.interpreter_error;
        error=m8rt_native_interpreter_service(rt,result);
        if(error && saved_error) error=saved_error;
    }
    rt->native.interpreting=false;
    m8_native_interpreter_profile_record(rt->interpreter_profile,&detail,error,arch->last_executed);
    m8_thumb_code_cache_finish(&rt->native.code_cache,detail.pc,detail.width,
                              cpu->r[15],boundary,result==0 && !error);
    rt->native.portable_guest+=arch->last_executed;
    rt->native.interpreter_error=error;
    if(error==256) {
        const m8_thumb_result *detail=m8_thumb_last_result();
        m8rt_fail(rt,"native fallback unsupported pc=%08x opcode=%08x width=%u family=%s",
            detail->pc,detail->opcode,detail->width,detail->family);
    }
    return error?1:0;
}

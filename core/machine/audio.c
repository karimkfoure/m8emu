#include "internal.h"
#include <fenv.h>
#include <string.h>

#define PRODUCER_ENTRY 0x33f9cu
#define PRODUCER_HEAD 0x33fdau
#define PRODUCER_RETURN 0x21828u
#define RING_HEAD 0x2003b464u
#define RING_FLAGS 0x20254dccu
#define RING_POOL 0x2025585cu
#define LOCAL_QUEUE_BYTES (64u * 256u)

/* The pinned formatter performs numeric min/max, binary32 multiplication by
 * 32767 and conversion toward zero. Classify NaN before host FP can quiet it. */
static int16_t sample(uint32_t bits) {
    if((bits&0x7f800000u)==0x7f800000u) {
        if(bits&0x7fffffu) return bits&0x400000u?32767:-32767;
        return bits&0x80000000u?-32767:32767;
    }
    float value; memcpy(&value,&bits,4);
    if(value>1.0f) value=1.0f;
    if(value< -1.0f) value= -1.0f;
    volatile float scaled=value*32767.0f;
    return (int16_t)scaled;
}

int m8rt_convert_audio_block(const uint8_t raw[512],const uint8_t flags[2],uint8_t pcm[256]) {
    if(!raw || !flags || !pcm || fegetround()!=FE_TONEAREST) return -1;
    for(unsigned frame=0;frame<64;frame++) for(unsigned channel=0;channel<2;channel++) {
        int16_t value=flags[channel]?sample(m8rt_le32(raw+(channel*64+frame)*4)):0;
        m8rt_st16(pcm+(frame*2+channel)*2,(uint16_t)value);
    }
    return 0;
}

static void local_hook(uc_engine *uc,uint64_t address,uint32_t size,void *opaque) {
    (void)size; m8rt *rt=opaque; uint64_t start=m8rt_callback_start(false);
    uint8_t head_raw[2];
    if(m8rt_mem_read(rt,RING_HEAD,head_raw,2)) {
        m8rt_fail(rt,"local tap head read failed pc=%05llx",(unsigned long long)address); goto done;
    }
    uint16_t head=m8rt_le16(head_raw);
    unsigned ipsr=m8rt_reg_read(rt,UC_ARM_REG_IPSR)&0x1ff;
    uint32_t sp=m8rt_reg_read(rt,UC_ARM_REG_SP);
    if(head>=71) { m8rt_fail(rt,"local tap invalid head pc=%05llx head=%u",(unsigned long long)address,head);goto done; }
    if(address==PRODUCER_ENTRY) {
        if(rt->local_phase) { m8rt_fail(rt,"local tap entry order phase=%u head=%u",rt->local_phase,head);goto done; }
        uint32_t lr=m8rt_reg_read(rt,UC_ARM_REG_LR), receiver=m8rt_reg_read(rt,UC_ARM_REG_R5);
        uint32_t writer=m8rt_reg_read(rt,UC_ARM_REG_R7);
        if(ipsr!=86) { m8rt_fail(rt,"local tap entry exception ipsr=%u head=%u",ipsr,head);goto done; }
        if(lr!=(PRODUCER_RETURN|1)) { m8rt_fail(rt,"local tap entry return lr=%08x head=%u",lr,head);goto done; }
        if(receiver<0x20000000u || receiver>0x2007ffbcu) { m8rt_fail(rt,"local tap entry receiver range receiver=%08x min=20000000 max=2007ffbc",receiver);goto done; }
        if(receiver&3) { m8rt_fail(rt,"local tap entry receiver alignment receiver=%08x mask=3",receiver);goto done; }
        if(writer!=receiver+0x30) { m8rt_fail(rt,"local tap entry writer writer=%08x receiver=%08x offset=30",writer,receiver);goto done; }
        /* Original publisher and caller must still be the executed firmware. */
        if(memcmp(rt->memory.itcm.host+PRODUCER_ENTRY,rt->memory.flash.host+0x1b4980,124)) {
            m8rt_fail(rt,"local tap changed publisher pc=%05x",PRODUCER_ENTRY);goto done;
        }
        rt->local_head=head; rt->local_sp=sp; rt->local_receiver=receiver;rt->local_writer=writer;
        int error=m8rt_mem_read(rt,RING_POOL+(uint32_t)head*512,rt->local_raw,512);
        if(error) { m8rt_fail(rt,"local tap publication sample read failed head=%u address=%08x error=%d",head,RING_POOL+(uint32_t)head*512,error);goto done; }
        error=m8rt_mem_read(rt,RING_FLAGS+(uint32_t)head*2,rt->local_flags,2);
        if(error) { m8rt_fail(rt,"local tap publication flag read failed head=%u address=%08x error=%d",head,RING_FLAGS+(uint32_t)head*2,error);goto done; }
        rt->local_phase=1;
    } else if(address==PRODUCER_HEAD) {
        if(rt->local_phase!=1) { m8rt_fail(rt,"local tap head order phase=%u head=%u",rt->local_phase,head);goto done; }
        if(head!=(rt->local_head+1)%71) { m8rt_fail(rt,"local tap head transition before=%u after=%u",rt->local_head,head);goto done; }
        if(ipsr!=86) { m8rt_fail(rt,"local tap head exception ipsr=%u expected=86",ipsr);goto done; }
        if(sp!=rt->local_sp-4) { m8rt_fail(rt,"local tap head stack sp=%08x entry_sp=%08x offset=-4",sp,rt->local_sp);goto done; }
        rt->local_phase=2;
    } else {
        /* A producer can return without publishing when no source is ready. */
        if(!rt->local_phase) goto done;
        if(rt->local_phase!=2) { m8rt_fail(rt,"local tap return order phase=%u head=%u",rt->local_phase,head);goto done; }
        if(head!=(rt->local_head+1)%71) { m8rt_fail(rt,"local tap return head transition head=%u old_head=%u modulus=71",head,rt->local_head);goto done; }
        if(ipsr!=86) { m8rt_fail(rt,"local tap return exception ipsr=%u expected=86",ipsr);goto done; }
        if(sp!=rt->local_sp) { m8rt_fail(rt,"local tap return stack sp=%08x entry_sp=%08x",sp,rt->local_sp);goto done; }
        rt->local_phase=0;
        if(rt->audit) m8rt_audit_audio(rt,(uint32_t)address);
        if(!rt->local_capture || !m8rt_usb_boot(rt)) goto done;
        if(rt->local_pcm.len-rt->local_pcm.head+256>LOCAL_QUEUE_BYTES) {
            m8rt_fail(rt,"local tap queue full queued=%zu capacity=%u blocks=%llu",rt->local_pcm.len-rt->local_pcm.head,LOCAL_QUEUE_BYTES,(unsigned long long)rt->local_blocks);goto done;
        }
        uint8_t pcm[256];
        if(m8rt_convert_audio_block(rt->local_raw,rt->local_flags,pcm)) { m8rt_fail(rt,"local tap needs FE_TONEAREST host rounding");goto done; }
        m8rt_audio_position position={rt->local_total/4,rt->guest_cycles,64,0};
        if(m8rt_queue_append(&rt->local_pcm,pcm,256) || m8rt_queue_append(&rt->local_positions,&position,sizeof position)) {
            m8rt_fail(rt,"local tap queue allocation failed");goto done;
        }
        rt->local_total+=256;rt->local_blocks++;rt->local_last_cycles=rt->guest_cycles;
    }
done:
    (void)uc;m8rt_callback_finish(rt,start,false);
}

#ifdef M8RT_NATIVE_ENGINE
void m8rt_native_audio_boundary(m8rt *rt,uint32_t pc) {local_hook(NULL,pc,0,rt);}
#endif

int m8rt_set_audio_tap(m8rt *rt,int local) {
    if(!rt || rt->fatal || rt->in_emu || rt->started) return -1;
    if(local && !rt->local_enabled) {
        const uint32_t pcs[]={PRODUCER_ENTRY,PRODUCER_HEAD,PRODUCER_RETURN};
        for(unsigned i=0;i<3;i++) {
            uc_err err=uc_hook_add(rt->uc,&rt->local_hooks[i],UC_HOOK_CODE,local_hook,rt,pcs[i],pcs[i]);
            if(err) { m8rt_fail(rt,"local tap hook pc=%05x: %s",pcs[i],uc_strerror(err));return -1; }
        }
        rt->local_enabled=true;
    }
    rt->local_selected=local!=0;
    return 0;
}

size_t m8rt_read_local_pcm(m8rt *rt,void *out,size_t n,m8rt_audio_position *position) {
    if(!rt || n<256 || rt->local_pcm.len-rt->local_pcm.head<256) return 0;
    m8rt_audio_position value;
    if(m8rt_queue_read(&rt->local_positions,&value,sizeof value)!=sizeof value) return 0;
    if(position) *position=value;
    return m8rt_queue_read(&rt->local_pcm,out,256);
}

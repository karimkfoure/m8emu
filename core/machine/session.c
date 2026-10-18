#include "session-internal.h"
#include "session-sha256.h"
#define HEADER 96u
#define FEATURES UINT64_C(7)
#define MEDIA_FEATURE UINT64_C(8)
#define NOR_OFFSET 0x7c0000u
#define NOR_SIZE (63u*4096u)
#define SECTION_COUNT 9u

static m8rt_session_result refused(m8rt_session_error *e,m8rt_session_result r,
                                   const char *p,uint64_t a,uint64_t b,uint32_t tag) {
    if(e) *e=(m8rt_session_error){p,a,b,tag};
    return r;
}
static void digest(const void *data,size_t n,uint8_t out[32]) {
    sha256 s;char hex[65];sha_init(&s);sha_update(&s,data,n);sha_finish(&s,hex);
    for(unsigned i=0;i<8;i++) {uint32_t h=s.h[i];for(unsigned j=0;j<4;j++)out[i*4+j]=(uint8_t)(h>>(24-8*j));}
}
static void immutable_digest(m8rt *rt,uint8_t out[32]) {
    sha256 s;char hex[65];sha_init(&s);
    sha_update(&s,rt->memory.flash.host,NOR_OFFSET);
    sha_update(&s,rt->memory.flash.host+NOR_OFFSET+NOR_SIZE,rt->memory.flash.size-NOR_OFFSET-NOR_SIZE);
    sha_update(&s,rt->rom,128*1024);sha_finish(&s,hex);
    for(unsigned i=0;i<8;i++)for(unsigned j=0;j<4;j++)out[i*4+j]=(uint8_t)(s.h[i]>>(24-8*j));
}
void m8rt_session_firmware_identity(m8rt *rt) {
    digest(rt->memory.flash.host,rt->memory.flash.size,rt->firmware_sha256);
    immutable_digest(rt,rt->immutable_sha256);
}
void m8rt_session_queue(m8_session_io *c,m8rt_queue *q,unsigned unit) {
    uint64_t n=session_number(c,q->len-q->head,8);
    if(n>SIZE_MAX || n%unit || (c->data && n>c->size-c->at)) {c->result=M8RT_SESSION_INVALID;return;}
    if(c->pass==2) {
        uint8_t *p=n?realloc(q->data,(size_t)n):NULL;
        if(n&&!p) {c->result=M8RT_SESSION_NOMEM;return;}
        if(!n) free(q->data);
        q->data=p;q->head=0;q->len=q->cap=(size_t)n;
    }
    if(c->raw) {session_bytes(c,q->data?q->data+q->head:NULL,(size_t)n);return;}
    if(unit==4) {
        for(size_t i=0;i<n&&!c->result;i+=4) {
            uint32_t word=0;
            if(!c->pass)memcpy(&word,q->data+q->head+i,4);
            word=(uint32_t)session_number(c,word,4);
            if(c->pass==2)memcpy(q->data+i,&word,4);
        }
        return;
    }
    if(unit!=sizeof(m8rt_audio_position)) {
        session_bytes(c,q->data?q->data+q->head:NULL,(size_t)n);return;
    }
    for(size_t i=0;i<n&&!c->result;i+=unit) {
        m8rt_audio_position unused={0},*p=c->pass==1?&unused:(m8rt_audio_position *)(q->data+q->head+i);
        S64(p->frame);S64(p->cycles);S32(p->frames);S32(p->reserved);
    }
}
void m8rt_session_words(m8_session_io *c,uint32_t *words,size_t capacity) {
    if(c->raw) {session_bytes(c,words,capacity*4);return;}
    uint64_t count=0;
    if(!c->pass)for(size_t i=0;i<capacity;i++)count+=words[i]!=0;
    count=session_number(c,count,4);
    if(count>capacity || (c->data && count>(c->size-c->at)/8)) {c->result=M8RT_SESSION_INVALID;return;}
    if(c->pass==2)memset(words,0,capacity*4);
    if(!c->pass) {
        for(size_t i=0;i<capacity;i++)if(words[i]) {session_number(c,i,4);session_number(c,words[i],4);}
    } else {
        uint64_t previous=0;
        for(size_t i=0;i<count&&!c->result;i++) {
            uint64_t index=session_number(c,0,4),value=session_number(c,0,4);
            if(index>=capacity || (i&&index<=previous) || !value) {c->result=M8RT_SESSION_INVALID;return;}
            if(c->pass==2)words[index]=(uint32_t)value;
            previous=index;
        }
    }
}
static void cpu_state(m8rt *rt,m8_session_io *c) {
#ifdef M8RT_PURE_NATIVE
    m8_cpu *p=&rt->native.cpu;m8_native_arch *a=&rt->native.arch;
    for(unsigned i=0;i<16;i++)S32(p->r[i]);
    S32(p->xpsr);for(unsigned i=0;i<32;i++)S32(p->s[i]);S32(p->fpscr);
    S32(a->primask);
    S32(a->basepri);
    S32(a->faultmask);
    S32(a->control);
    S32(a->msp);
    S32(a->psp);
    S32(a->ipsr);
    S32(a->itstate);
    S32(a->exclusive_address);
    S32(a->exclusive_valid);
    S32(a->exclusive_value);
    S32(a->cpsr);
    S32(a->fpexc);
    S32(a->last_executed);
    S32(a->last_special);
    S32(rt->native.cpsr);
#else
    (void)rt;c->result=M8RT_SESSION_UNSUPPORTED;
#endif
}
static void runtime_state(m8rt *rt,m8_session_io *c) {
    S32(rt->options.reference_policy);
    S32(rt->options.strict);
    S32(rt->options.host_handshake);
    S64(rt->boot_main_start);
    S32(rt->cyccnt);
    S32(rt->vtor);
    S32(rt->syst_csr);
    S32(rt->syst_rvr);
    S32(rt->syst_cvr);
    for(unsigned i=0;i<8;i++) {S32(rt->nvic_iser[i]);S32(rt->nvic_ispr[i]);}
    for(unsigned i=0;i<64;i++)S32(rt->nvic_ipr[i]);
    for(unsigned i=0;i<256;i++) {S64(rt->nvic_pend_seq[i]);S64(rt->nvic_isr_done[i]);}
    S32(rt->nvic_pending_banks);
    S32(rt->next_systick);
    S32(rt->next_sof);
    S32(rt->next_audio);
    SB(rt->systick_pending);
    SB(rt->audio_pending);
    SB(rt->usb_pending);
    SB(rt->entry_pending);
    SB(rt->er_sent);
    SI32(rt->audio_ch);
    SI32(rt->irq_nest);
    for(unsigned i=0;i<32;i++)S32(rt->exc_stack[i]);
    SB(rt->sdhc_in_sync);
    SB(rt->started);
    SB(rt->aot_sdhc_pending);
    SB(rt->sampling_pending);
    S64(rt->main_iters);
    S64(rt->slices);
    S64(rt->pcm_total);
    S64(rt->cdc_total);
    S64(rt->main_selected);
    S64(rt->main_expanded);
    S64(rt->main_cycles);
    S64(rt->guest_cycles);
    S64(rt->local_total);
    S64(rt->local_blocks);
    S64(rt->local_last_cycles);
    SB(rt->local_enabled);
    SB(rt->local_selected);
    SB(rt->local_capture);
    S32(rt->local_phase);
    S16(rt->local_head);
    S32(rt->local_sp);
    S32(rt->local_receiver);
    S32(rt->local_writer);
    S32(rt->entry);
    S32(rt->last_pc);
    session_bytes(c,rt->local_raw,sizeof rt->local_raw);session_bytes(c,rt->local_flags,2);
    S32(rt->pcm_packet_left);
    S64(rt->pcm_position.frame);
    S64(rt->pcm_position.cycles);
    S32(rt->pcm_position.frames);
    S32(rt->pcm_position.reserved);
    m8rt_session_queue(c,&rt->cdc,1);
    m8rt_session_queue(c,&rt->pcm,1);
    m8rt_session_queue(c,&rt->irqs,4);
    m8rt_session_queue(c,&rt->pcm_lengths,1);
    m8rt_session_queue(c,&rt->pcm_positions,sizeof(m8rt_audio_position));
    m8rt_session_queue(c,&rt->local_pcm,1);
    m8rt_session_queue(c,&rt->local_positions,sizeof(m8rt_audio_position));
    S64(rt->watchdog.deadline_cycles);
    S64(rt->watchdog.last_refresh_cycles);
    S64(rt->watchdog.refreshes);
    S64(rt->watchdog.rejected_services);
    S64(rt->watchdog.interrupts);
    S64(rt->watchdog.resets);
    S64(rt->watchdog.last_reset_cycles);
    S64(rt->watchdog.fallback_cycles);
    S64(rt->watchdog.fallback_slots);
    S64(rt->watchdog.idle_batches);
    S32(rt->watchdog.control);
    S32(rt->watchdog.service);
    S32(rt->watchdog.reset_status);
    S32(rt->watchdog.interrupt_control);
    S32(rt->watchdog.miscellaneous);
    S32(rt->watchdog.enabled);
    S32(rt->watchdog.reset_pending);
    SB(rt->watchdog_service_armed);
    SB(rt->watchdog_control_locked);
    SB(rt->watchdog_interrupt_locked);
    SB(rt->watchdog_warned);
}
/* Section order is fixed for canonical capture. Unknown optional host sections
 * are carried by adapters; unknown required features or sections are refused. */
static void encode(m8rt *rt,uint32_t tag,m8_session_io *c) {
    switch(tag) {
    case 1:cpu_state(rt,c);break;
    case 2:
        session_bytes(c,rt->memory.itcm.host,512*1024);
        session_bytes(c,rt->memory.dtcm.host,512*1024);
        session_bytes(c,rt->memory.ocram.host,512*1024);
        session_bytes(c,rt->dwt,4096);break;
    case 3:runtime_state(rt,c);break;
    case 4:m8rt_session_mmio(rt,c);break;
    case 5:m8rt_session_usb(rt,c);break;
    case 6:m8rt_session_sd(rt,c);break;
    case 7:
        if(c->pass==2) {
            if(m8rt_sd_replace_image(rt,c->data+c->at,c->size-c->at))c->result=M8RT_SESSION_NOMEM;
            c->at=c->size;
        } else if(c->pass) c->at=c->size;
        else c->at+=m8rt_sd_image_size(rt);
        break;
    case 8:m8rt_session_nor(rt,c);break;
    case 9:session_bytes(c,rt->memory.flash.host+NOR_OFFSET,NOR_SIZE);break;
    default:c->result=M8RT_SESSION_INVALID;break;
    }
}
static m8rt_session_result boundary(m8rt *rt,m8rt_session_error *e,bool media,bool raw) {
    if(!rt || !rt->sd || !rt->nor)return refused(e,M8RT_SESSION_INVALID,"runtime",0,1,0);
    if(rt->fatal)return refused(e,M8RT_SESSION_GUEST_FAILED,"guest_failed",1,0,0);
    if(rt->in_emu)return refused(e,M8RT_SESSION_BUSY,"execution_active",1,0,0);
    if(media) {
        m8rt_session_result r=m8rt_session_sd_media(rt,e);if(r)return r;
        if(m8rt_session_nor_busy(rt))return refused(e,M8RT_SESSION_BUSY,"nor_pending",1,0,9);
        return M8RT_SESSION_OK;
    }
#ifndef M8RT_PURE_NATIVE
    return refused(e,M8RT_SESSION_UNSUPPORTED,"native_engine",0,1,1);
#else
    if(!rt->started || !m8rt_usb_boot(rt))return refused(e,M8RT_SESSION_BUSY,"boot_complete",0,1,1);
    if(!rt->deterministic_clock || !rt->native.active || rt->native_live.enabled)
        return refused(e,M8RT_SESSION_UNSUPPORTED,"deterministic_native_owner",0,1,1);
#define BUSY(field) if(rt->field)return refused(e,M8RT_SESSION_BUSY,#field,rt->field,0,1)
    BUSY(native.code_cache.active);BUSY(native.interpreting);
    BUSY(aot_active);BUSY(native.boot_callback_irq);BUSY(native_boot_app.bus_pending);
    BUSY(native_boot_app.bus_executing);BUSY(native_boot_app.pending);BUSY(native_boot_app.ready);
    BUSY(native_live.enter_pending);BUSY(native_live.leave_pending);
#undef BUSY
    if(rt->audit) {
        m8rt_audit_status status;
        if(m8rt_get_audit_status(rt,&status) || status.enabled)
            return refused(e,M8RT_SESSION_UNSUPPORTED,"audit_enabled",1,0,3);
    }
    uint8_t immutable[32];
    if(!raw)immutable_digest(rt,immutable);
    if(rt->session_immutable_changed || (!raw && memcmp(immutable,rt->immutable_sha256,32)))return refused(e,M8RT_SESSION_UNSUPPORTED,"immutable_memory",1,0,2);
    return M8RT_SESSION_OK;
#endif
}
static m8rt_session_result capture(m8rt *rt,const m8rt_session_section *host,size_t count,
    m8rt_session_blob *out,m8rt_session_error *e,bool media,bool checked) {
    if(e)*e=(m8rt_session_error){0};
    if(!out)return refused(e,M8RT_SESSION_INVALID,"output",0,1,0);
    *out=(m8rt_session_blob){0};
    m8rt_session_result r=checked?M8RT_SESSION_OK:boundary(rt,e,media,false);if(r)return r;
    if(count>55 || (count&&!host))return refused(e,M8RT_SESSION_INVALID,"host_sections",count,55,0);
    size_t lengths[9]={0},total=HEADER;
    for(uint32_t tag=1;tag<=SECTION_COUNT;tag++) {
        if(media&&tag!=7&&tag!=9)continue;
        m8_session_io c={0};encode(rt,tag,&c);
        if(c.result)return refused(e,c.result,"encode_size",c.at,0,tag);
        lengths[tag-1]=c.at;
        if(c.at>SIZE_MAX-total-16)return refused(e,M8RT_SESSION_NOMEM,"container_size",c.at,SIZE_MAX-total-16,tag);
        total+=16+c.at;
    }
    for(size_t i=0;i<count;i++) {
        if(host[i].tag<0x10000 || (host[i].size&&!host[i].data) || host[i].required>1)
            return refused(e,M8RT_SESSION_INVALID,"host_section",host[i].tag,0x10000,host[i].tag);
        for(size_t j=0;j<i;j++)if(host[i].tag==host[j].tag)return refused(e,M8RT_SESSION_INVALID,"duplicate_section",host[i].tag,0,host[i].tag);
        if(host[i].size>SIZE_MAX-total-16)return refused(e,M8RT_SESSION_NOMEM,"host_size",host[i].size,SIZE_MAX-total-16,host[i].tag);
        total+=16+host[i].size;
    }
    uint8_t *data=malloc(total);if(!data)return refused(e,M8RT_SESSION_NOMEM,"blob_allocation",total,0,0);
    memset(data,0,HEADER);memcpy(data,"M8RTSES1",8);
    m8_session_io c={.data=data,.size=total,.at=8};
    session_number(&c,1,4);session_number(&c,(media?2:SECTION_COUNT)+count,4);
    session_number(&c,total,8);session_number(&c,media?MEDIA_FEATURE:FEATURES,8);
    session_bytes(&c,rt->firmware_sha256,32);c.at=HEADER;
    for(uint32_t tag=1;tag<=SECTION_COUNT;tag++) {
        if(media&&tag!=7&&tag!=9)continue;
        session_number(&c,tag,4);session_number(&c,1,4);session_number(&c,lengths[tag-1],8);
        m8_session_io section={.data=data+c.at,.size=lengths[tag-1]};
        if(tag==7) {
            if(m8rt_sd_copy_image(rt,section.data,section.size))section.result=M8RT_SESSION_INVALID;
            section.at=section.size;
        } else encode(rt,tag,&section);
        if(section.result||section.at!=section.size) {free(data);return refused(e,section.result?section.result:M8RT_SESSION_INVALID,"encode_length",section.at,section.size,tag);}
        c.at+=section.size;
    }
    for(size_t i=0;i<count;i++) {
        session_number(&c,host[i].tag,4);session_number(&c,host[i].required,4);session_number(&c,host[i].size,8);
        session_bytes(&c,(void *)host[i].data,host[i].size);
    }
    digest(data+HEADER,total-HEADER,data+64);
    out->data=data;out->size=total;return M8RT_SESSION_OK;
}
m8rt_session_result m8rt_session_capture(m8rt *rt,const m8rt_session_section *h,size_t n,m8rt_session_blob *b,m8rt_session_error *e) {return capture(rt,h,n,b,e,false,false);}
m8rt_session_result m8rt_media_capture(m8rt *rt,m8rt_session_blob *b,m8rt_session_error *e) {return capture(rt,NULL,0,b,e,true,false);}
void m8rt_session_release(m8rt_session_blob *b) {if(b){free((void *)b->data);*b=(m8rt_session_blob){0};}}

typedef struct {m8rt_session_info info;m8rt_session_section sections[64];} session_layout;
static m8rt_session_result layout(const void *input,size_t size,session_layout *l,m8rt_session_error *e,bool verify_hash) {
    if(e)*e=(m8rt_session_error){0};
    if(!input || size<HEADER)return refused(e,M8RT_SESSION_INVALID,"header_length",size,HEADER,0);
    const uint8_t *data=input;
    if(memcmp(data,"M8RTSES1",8))return refused(e,M8RT_SESSION_INVALID,"magic",0,1,0);
    m8_session_io c={.data=(uint8_t *)input,.size=size,.at=8,.pass=1};
    l->info.version=(uint32_t)session_number(&c,0,4);
    l->info.sections=(uint32_t)session_number(&c,0,4);
    l->info.bytes=session_number(&c,0,8);l->info.required_features=session_number(&c,0,8);
    memcpy(l->info.firmware_sha256,data+32,32);
    if(l->info.version!=1)return refused(e,M8RT_SESSION_INCOMPATIBLE,"schema_version",l->info.version,1,0);
    if(l->info.bytes!=size)return refused(e,M8RT_SESSION_INVALID,"container_length",size,l->info.bytes,0);
    if(l->info.required_features!=FEATURES && l->info.required_features!=MEDIA_FEATURE)
        return refused(e,M8RT_SESSION_INCOMPATIBLE,"required_features",l->info.required_features,FEATURES|MEDIA_FEATURE,0);
    if(l->info.sections>64)return refused(e,M8RT_SESSION_INVALID,"section_count",l->info.sections,64,0);
    if(verify_hash) {
        uint8_t hash[32];digest(data+HEADER,size-HEADER,hash);
        if(memcmp(hash,data+64,32))return refused(e,M8RT_SESSION_INVALID,"payload_sha256",0,1,0);
    }
    c.at=HEADER;uint32_t found=0;
    for(unsigned i=0;i<l->info.sections;i++) {
        uint32_t tag=(uint32_t)session_number(&c,0,4),required=(uint32_t)session_number(&c,0,4);
        uint64_t n=session_number(&c,0,8);
        if(c.result||n>size-c.at||required>1)return refused(e,M8RT_SESSION_INVALID,"section_length",n,size-c.at,tag);
        if((!tag||tag>9) && tag<0x10000 && required)return refused(e,M8RT_SESSION_INCOMPATIBLE,"required_section",tag,9,tag);
        /* Hosts validate their own required tags before activating a restore. */
        if(required && tag>=0x10000 && tag!=M8RT_SESSION_WEB && tag!=M8RT_SESSION_SDL)
            return refused(e,M8RT_SESSION_INCOMPATIBLE,"required_host_section",tag,0,tag);
        for(unsigned j=0;j<i;j++)if(l->sections[j].tag==tag)return refused(e,M8RT_SESSION_INVALID,"duplicate_section",tag,0,tag);
        l->sections[i]=(m8rt_session_section){tag,required,data+c.at,(size_t)n};
        if(tag>=1&&tag<=9)found|=1u<<(tag-1);
        c.at+=(size_t)n;
    }
    uint32_t expected=l->info.required_features==MEDIA_FEATURE?(1u<<6)|(1u<<8):0x1ffu;
    if(c.at!=size)return refused(e,M8RT_SESSION_INVALID,"trailing_bytes",size-c.at,0,0);
    if((found&expected)!=expected)return refused(e,M8RT_SESSION_INVALID,"missing_sections",found,expected,0);
    return M8RT_SESSION_OK;
}
m8rt_session_result m8rt_session_describe(const void *d,size_t n,m8rt_session_info *out,m8rt_session_error *e) {
    session_layout l; m8rt_session_result r=layout(d,n,&l,e,true);if(!r&&out)*out=l.info;return r;
}
m8rt_session_result m8rt_session_find(const void *d,size_t n,uint32_t tag,m8rt_session_section *out,m8rt_session_error *e) {
    session_layout l;m8rt_session_result r=layout(d,n,&l,e,true);if(r)return r;
    for(unsigned i=0;i<l.info.sections;i++)if(l.sections[i].tag==tag){if(out)*out=l.sections[i];return M8RT_SESSION_OK;}
    return refused(e,M8RT_SESSION_INVALID,"section_absent",tag,0,tag);
}
m8rt_session_result m8rt_session_blob_find(const m8rt_session_blob *blob,uint32_t tag,m8rt_session_section *out,m8rt_session_error *e) {
    session_layout l;
    if(!blob)return refused(e,M8RT_SESSION_INVALID,"capture_blob",0,1,tag);
    m8rt_session_result r=layout(blob->data,blob->size,&l,e,false);if(r)return r;
    for(unsigned i=0;i<l.info.sections;i++)if(l.sections[i].tag==tag){if(out)*out=l.sections[i];return M8RT_SESSION_OK;}
    return refused(e,M8RT_SESSION_INVALID,"section_absent",tag,0,tag);
}
#ifdef M8RT_PURE_NATIVE
extern int m8rt_native_session_rebuild(m8rt *);
extern int m8rt_native_session_prepare(m8rt *);
#endif
m8rt_session_result m8rt_session_restore(m8rt *rt,const void *d,size_t n,m8rt_session_error *e) {
    session_layout l;m8rt_session_result r=layout(d,n,&l,e,true);if(r)return r;
    if(l.info.required_features!=FEATURES)return refused(e,M8RT_SESSION_INCOMPATIBLE,"machine_features",l.info.required_features,FEATURES,0);
    if(!rt||rt->fatal)return refused(e,M8RT_SESSION_GUEST_FAILED,"runtime",0,1,0);
    if(rt->in_emu||rt->started)return refused(e,M8RT_SESSION_BUSY,"fresh_runtime",rt->started,0,0);
    if(memcmp(rt->firmware_sha256,l.info.firmware_sha256,32))return refused(e,M8RT_SESSION_INCOMPATIBLE,"firmware_sha256",0,1,0);
#ifndef M8RT_PURE_NATIVE
    return refused(e,M8RT_SESSION_UNSUPPORTED,"native_engine",0,1,1);
#else
    /* Validate every section before installation. Resource/engine failures after
     * installation leave a disposable candidate, never an activated runtime. */
    for(unsigned pass=1;pass<=2;pass++) {
        if(pass==2) {
            if(m8rt_native_session_prepare(rt))return refused(e,M8RT_SESSION_INCOMPATIBLE,"engine_prepare",0,1,1);
            for(unsigned i=0;i<l.info.sections;i++)if(l.sections[i].tag==7) {
                const m8rt_session_section *s=&l.sections[i];
                if(m8rt_sd_replace_image(rt,s->data,s->size))return refused(e,M8RT_SESSION_NOMEM,"media_install",s->size,0,7);
            }
        }
        for(unsigned i=0;i<l.info.sections;i++) {
            m8rt_session_section *s=&l.sections[i];if(s->tag<1||s->tag>9||(pass==2&&s->tag==7))continue;
            if((s->tag==7&&(!s->size||s->size%512)) || (s->tag==9&&s->size!=NOR_SIZE))
                return refused(e,M8RT_SESSION_INVALID,"media_length",s->size,s->tag==9?NOR_SIZE:512,s->tag);
            m8_session_io c={.data=(uint8_t *)s->data,.size=s->size,.pass=pass};encode(rt,s->tag,&c);
            if(c.result||c.at!=c.size)return refused(e,c.result?c.result:M8RT_SESSION_INVALID,"section_schema",c.at,c.size,s->tag);
        }
    }
    if(rt->irq_nest<0||rt->irq_nest>32||rt->local_phase>2||rt->local_head>=71)
        return refused(e,M8RT_SESSION_INVALID,"machine_ranges",rt->irq_nest,32,3);
    if(m8rt_native_session_rebuild(rt))return refused(e,M8RT_SESSION_INCOMPATIBLE,"engine_rebuild",0,1,1);
    atomic_store(&rt->host_stop,false);return M8RT_SESSION_OK;
#endif
}

/* The transient packet contains copied values and dense MMIO, with no engine
 * pointers. Only the storage worker turns it into the portable container. */
typedef struct {
    char magic[8];
    uint32_t version,endian;
    uint64_t total,sd_size,nor_size,sd_revision,nor_revision;
    uint8_t firmware[32];
    uint64_t lengths[9];
} raw_header;
#define MEDIA_CHUNK 65536u
typedef struct {
    atomic_uint refs;
    size_t size;
    uint8_t bytes[];
} raw_chunk;
typedef struct {
    atomic_uint refs;
    size_t size,count,changed_count;
    uint64_t base_revision;
    raw_chunk **chunks;
    size_t *changed;
    _Atomic(uint8_t *) flat;
} raw_media;
struct m8rt_session_raw {
    atomic_uint refs;
    const m8rt *owner;
    m8rt_session_blob packet;
    raw_media *sd,*nor;
    uint64_t sd_revision,nor_revision;
};
uint64_t m8rt_nor_image_revision(m8rt *rt) {return rt?rt->session_nor_revision:0;}
static void media_drop(raw_media *m) {
    if(!m||atomic_fetch_sub(&m->refs,1)!=1)return;
    for(size_t i=0;i<m->count;i++)if(m->chunks[i]&&atomic_fetch_sub(&m->chunks[i]->refs,1)==1)free(m->chunks[i]);
    free(atomic_load(&m->flat));free(m->chunks);free(m->changed);free(m);
}
void m8rt_session_raw_retain(m8rt_session_raw *r) {if(r)atomic_fetch_add(&r->refs,1);}
void m8rt_session_raw_release(m8rt_session_raw *r) {
    if(!r || atomic_fetch_sub(&r->refs,1)!=1)return;
    media_drop(r->sd);media_drop(r->nor);m8rt_session_release(&r->packet);free(r);
}
m8rt_session_blob m8rt_session_raw_packet(const m8rt_session_raw *r) {return r?r->packet:(m8rt_session_blob){0};}
static raw_media *raw_medium(const m8rt_session_raw *r,uint32_t tag) {return !r?NULL:tag==7?r->sd:tag==9?r->nor:NULL;}
size_t m8rt_session_raw_media_size(const m8rt_session_raw *r,uint32_t tag) {raw_media *m=raw_medium(r,tag);return m?m->size:0;}
m8rt_session_blob m8rt_session_raw_media(const m8rt_session_raw *r,uint32_t tag) {
    raw_media *m=raw_medium(r,tag);if(!m)return (m8rt_session_blob){0};
    uint8_t *flat=atomic_load(&m->flat);
    if(!flat) {
        uint8_t *copy=malloc(m->size);if(!copy)return (m8rt_session_blob){0};
        for(size_t i=0;i<m->count;i++)memcpy(copy+i*MEDIA_CHUNK,m->chunks[i]->bytes,m->chunks[i]->size);
        if(atomic_compare_exchange_strong(&m->flat,&flat,copy))flat=copy;else free(copy);
    }
    return (m8rt_session_blob){flat,m->size};
}
/* The adapter may drop an assembled view while paused after seeding JS media.
 * No reader or encoder may hold its pointer across this explicit operation. */
void m8rt_session_raw_forget_media(m8rt_session_raw *r,uint32_t tag) {
    raw_media *m=raw_medium(r,tag);if(m)free(atomic_exchange(&m->flat,NULL));
}
size_t m8rt_session_raw_changes(const m8rt_session_raw *r,uint32_t tag) {raw_media *m=raw_medium(r,tag);return m?m->changed_count:0;}
uint64_t m8rt_session_raw_base_revision(const m8rt_session_raw *r,uint32_t tag) {raw_media *m=raw_medium(r,tag);return m?m->base_revision:0;}
m8rt_session_blob m8rt_session_raw_change(const m8rt_session_raw *r,uint32_t tag,size_t index,size_t *offset) {
    raw_media *m=raw_medium(r,tag);if(!m||index>=m->changed_count)return (m8rt_session_blob){0};
    size_t page=m->changed[index];raw_chunk *chunk=m->chunks[page];
    if(offset)*offset=page*MEDIA_CHUNK;
    return (m8rt_session_blob){chunk->bytes,chunk->size};
}
uint64_t m8rt_session_raw_revision(const m8rt_session_raw *r,uint32_t tag) {
    return !r?0:tag==7?r->sd_revision:tag==9?r->nor_revision:0;
}
static raw_media *media_copy(m8rt *rt,const m8rt_session_raw *old,uint32_t tag,uint64_t revision) {
    size_t n=tag==7?m8rt_sd_image_size(rt):NOR_SIZE;
    raw_media *previous=old&&old->owner==rt?raw_medium(old,tag):NULL;
    uint64_t before=m8rt_session_raw_revision(old,tag);
    if(previous && previous->size==n && before==revision) {atomic_fetch_add(&previous->refs,1);return previous;}
    raw_media *m=calloc(1,sizeof *m);if(!m)return NULL;
    atomic_init(&m->refs,1);atomic_init(&m->flat,NULL);m->size=n;m->count=n/MEDIA_CHUNK+(n%MEDIA_CHUNK!=0);m->base_revision=before;
    m->chunks=calloc(m->count,sizeof *m->chunks);m->changed=malloc(m->count*sizeof *m->changed);
    if(!m->chunks||!m->changed){free(m->chunks);free(m->changed);free(m);return NULL;}
    for(size_t i=0;i<m->count;i++) {
        size_t offset=i*MEDIA_CHUNK,part=n-offset;if(part>MEDIA_CHUNK)part=MEDIA_CHUNK;
        if(tag==7 && previous && previous->size==n && !m8rt_session_sd_page_changed(rt,offset,before)) {
            m->chunks[i]=previous->chunks[i];atomic_fetch_add(&m->chunks[i]->refs,1);continue;
        }
        raw_chunk *chunk=malloc(sizeof *chunk+part);if(!chunk){media_drop(m);return NULL;}
        atomic_init(&chunk->refs,1);chunk->size=part;m->chunks[i]=chunk;m->changed[m->changed_count++]=i;
        if(tag==7) {if(m8rt_sd_read_image(rt,offset,chunk->bytes,part)){media_drop(m);return NULL;}}
        else memcpy(chunk->bytes,rt->memory.flash.host+NOR_OFFSET+offset,part);
    }
    return m;
}
static m8rt_session_result raw_copy(m8rt *rt,const m8rt_session_raw *old,
    m8rt_session_raw **out,m8rt_session_error *e,bool seed) {
    if(e)*e=(m8rt_session_error){0};
    if(!out)return refused(e,M8RT_SESSION_INVALID,"output",0,1,0);
    *out=NULL;
    m8rt_session_result result=boundary(rt,e,seed,true);if(result)return result;
    const uint16_t endian=1;
    if(*(const uint8_t *)&endian!=1)return refused(e,M8RT_SESSION_UNSUPPORTED,"raw_byte_order",0,1,0);
    m8rt_session_raw *r=calloc(1,sizeof *r);if(!r)return M8RT_SESSION_NOMEM;
    atomic_init(&r->refs,1);r->owner=rt;r->sd_revision=m8rt_sd_image_revision(rt);r->nor_revision=rt->session_nor_revision;
    r->sd=media_copy(rt,old,7,r->sd_revision);r->nor=media_copy(rt,old,9,r->nor_revision);
    if(!r->sd||!r->nor){m8rt_session_raw_release(r);return M8RT_SESSION_NOMEM;}
    if(seed){*out=r;return M8RT_SESSION_OK;}
    raw_header h={.magic={'M','8','R','T','R','A','W','1'},.version=1,.endian=1,.total=sizeof h,
        .sd_size=r->sd->size,.nor_size=r->nor->size,.sd_revision=r->sd_revision,.nor_revision=r->nor_revision};
    memcpy(h.firmware,rt->firmware_sha256,32);
    for(unsigned tag=1;tag<=9;tag++)if(tag!=7&&tag!=9) {
        m8_session_io c={.raw=1};encode(rt,tag,&c);
        if(c.result||c.at>SIZE_MAX-h.total){m8rt_session_raw_release(r);return M8RT_SESSION_NOMEM;}
        h.lengths[tag-1]=c.at;h.total+=c.at;
    }
    uint8_t *p=malloc((size_t)h.total);if(!p){m8rt_session_raw_release(r);return M8RT_SESSION_NOMEM;}
    memcpy(p,&h,sizeof h);size_t at=sizeof h;
    for(unsigned tag=1;tag<=9;tag++)if(tag!=7&&tag!=9) {
        m8_session_io c={.data=p+at,.size=(size_t)h.lengths[tag-1],.raw=1};encode(rt,tag,&c);
        if(c.result||c.at!=c.size){free(p);m8rt_session_raw_release(r);return refused(e,M8RT_SESSION_INVALID,"raw_copy_length",c.at,c.size,tag);}
        at+=c.size;
    }
    r->packet=(m8rt_session_blob){p,(size_t)h.total};*out=r;return M8RT_SESSION_OK;
}
m8rt_session_result m8rt_session_raw_seed(m8rt *rt,m8rt_session_raw **out,m8rt_session_error *e) {return raw_copy(rt,NULL,out,e,true);}
m8rt_session_result m8rt_session_raw_capture(m8rt *rt,const m8rt_session_raw *old,m8rt_session_raw **out,m8rt_session_error *e) {return raw_copy(rt,old,out,e,false);}

m8rt_session_result m8rt_session_raw_encode(const void *packet,size_t size,
    const void *sd,size_t sd_size,const void *nor,size_t nor_size,
    const m8rt_session_section *host,size_t count,m8rt_session_blob *out,m8rt_session_error *e) {
    if(e)*e=(m8rt_session_error){0};
    if(!out)return refused(e,M8RT_SESSION_INVALID,"output",0,1,0);
    *out=(m8rt_session_blob){0};
    raw_header h;
    if(!packet||size<sizeof h)return refused(e,M8RT_SESSION_INVALID,"raw_header",size,sizeof h,0);
    memcpy(&h,packet,sizeof h);
    if(memcmp(h.magic,"M8RTRAW1",8)||h.version!=1||h.endian!=1)return refused(e,M8RT_SESSION_INCOMPATIBLE,"raw_codec",h.version,1,0);
    if(h.total!=size||h.sd_size!=sd_size||h.nor_size!=nor_size||!sd||!sd_size||sd_size%512||!nor||nor_size!=NOR_SIZE)
        return refused(e,M8RT_SESSION_INVALID,"raw_media_length",sd_size,h.sd_size,7);
    size_t total=sizeof h;
    for(unsigned i=0;i<9;i++) {
        if((i==6||i==8)&&h.lengths[i])return M8RT_SESSION_INVALID;
        if(h.lengths[i]>size-total)return M8RT_SESSION_INVALID;
        total+=(size_t)h.lengths[i];
    }
    if(total!=size)return M8RT_SESSION_INVALID;
    /* This view has no CPU engine, firmware image, hooks, paths or caches. */
    m8rt *view=calloc(1,sizeof *view);if(!view)return M8RT_SESSION_NOMEM;
    uint8_t *ram=calloc(1,3u*512*1024+4096),*flash=calloc(1,8u*1024*1024);
    view->memory.itcm.host=ram;view->memory.dtcm.host=ram?ram+512*1024:NULL;
    view->memory.ocram.host=ram?ram+2*512*1024:NULL;view->dwt=ram?ram+3*512*1024:NULL;
    view->memory.flash.host=flash;memcpy(view->firmware_sha256,h.firmware,32);
    m8rt_session_result result=M8RT_SESSION_NOMEM;
    if(!ram||!flash||m8rt_mmio_init(view)||m8rt_usb_init(view)||m8rt_nor_init(view)||m8rt_session_sd_view(view,sd,sd_size))goto done;
    memcpy(flash+NOR_OFFSET,nor,nor_size);
    total=sizeof h;
    for(unsigned tag=1;tag<=9;tag++)if(tag!=7&&tag!=9) {
        m8_session_io c={.data=(uint8_t *)packet+total,.size=(size_t)h.lengths[tag-1],.pass=2,.raw=1};
        encode(view,tag,&c);total+=c.size;
        if(c.result||c.at!=c.size){result=refused(e,c.result?c.result:M8RT_SESSION_INVALID,"raw_section",c.at,c.size,tag);goto done;}
    }
    result=capture(view,host,count,out,e,false,true);
done:
    m8rt_mmio_destroy(view);m8rt_usb_destroy(view);m8rt_nor_destroy(view);m8rt_sd_destroy(view);
    m8rt_queue *queues[]={&view->cdc,&view->pcm,&view->irqs,&view->pcm_lengths,&view->pcm_positions,&view->local_pcm,&view->local_positions};
    for(unsigned i=0;i<sizeof queues/sizeof *queues;i++)free(queues[i]->data);
    free(ram);free(flash);free(view);return result;
}

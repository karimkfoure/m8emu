#include "cpu-backend.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE UINT64_C(4096)
#define ADDRESS_LIMIT (UINT64_C(1)<<32)
#define APSR_FLAGS UINT32_C(0xf8000000)
#define APSR_GE UINT32_C(0x000f0000)
#define EPSR_BITS UINT32_C(0x0700fc00)
#define IPSR_BITS UINT32_C(0x000001ff)
#define CPSR_SHARED UINT32_C(0xfe0ffc00)

typedef struct backing {
    uint8_t *bytes;
    size_t refs;
    bool owned;
} backing;
typedef struct mapping {
    uint64_t begin,end;
    unsigned perms;
    uint8_t *host;
    backing *storage;
    m8_cpu_backend_mmio_read_fn read;
    m8_cpu_backend_mmio_write_fn write;
    void *read_user,*write_user;
    uint64_t mmio_base;
} mapping;
typedef struct hook_record {
    m8_cpu_backend_hook id;
    int type;
    void *callback,*user;
    uint64_t begin,end;
    struct hook_record *next;
} hook_record;
struct m8_cpu_backend {
    m8_cpu local_cpu,*cpu;
    m8_native_arch local_arch,*arch;
    uint32_t local_cpsr,*cpsr;
    uint64_t exclusive[3];
    uint32_t exclusive_shadow[3];
    mapping *maps;
    size_t map_count;
    hook_record *hooks;
    m8_cpu_backend_hook next_hook;
    uint64_t generation;
    atomic_bool stop;
    bool bound;
};
struct m8_cpu_backend_context {
    m8_cpu cpu;
    m8_native_arch arch;
    uint32_t cpsr;
    uint64_t exclusive[3];
    bool valid;
};

static bool using_psp(const m8_native_arch *a) {return !a->ipsr && (a->control&2u);}
static bool privileged_register(int id) {
    return id==M8RT_REG_MSP || id==M8RT_REG_PSP || id==M8RT_REG_PRIMASK ||
        id==M8RT_REG_BASEPRI || id==M8RT_REG_BASEPRI_MAX || id==M8RT_REG_FAULTMASK;
}
static void save_sp(m8_cpu_backend *b) {
    if(using_psp(b->arch)) b->arch->psp=b->cpu->r[13];
    else b->arch->msp=b->cpu->r[13];
}
static void select_sp(m8_cpu_backend *b,bool previous) {
    if(previous!=using_psp(b->arch))
        b->cpu->r[13]=using_psp(b->arch)?b->arch->psp:b->arch->msp;
}
static void publish_psr(m8_cpu_backend *b) {
    b->arch->itstate=m8_native_it_from_xpsr(b->cpu->xpsr);
    *b->cpsr=(*b->cpsr&~0x20u)|((b->cpu->xpsr&0x01000000u)?0x20u:0);
    b->arch->cpsr=(*b->cpsr&~CPSR_SHARED)|(b->cpu->xpsr&CPSR_SHARED);
}
static void monitor_shadow(m8_cpu_backend *b) {
    b->exclusive_shadow[0]=b->arch->exclusive_address;
    b->exclusive_shadow[1]=b->arch->exclusive_value;
    b->exclusive_shadow[2]=b->arch->exclusive_valid;
}
static void monitor_sync(m8_cpu_backend *b) {
    if(b->exclusive_shadow[0]!=b->arch->exclusive_address ||
       b->exclusive_shadow[1]!=b->arch->exclusive_value ||
       b->exclusive_shadow[2]!=b->arch->exclusive_valid) {
        b->exclusive[0]=b->arch->exclusive_valid?b->arch->exclusive_address:UINT64_MAX;
        b->exclusive[1]=b->arch->exclusive_value;
        monitor_shadow(b);
    }
}
m8_cpu_backend_error m8_cpu_backend_open(int arch,int mode,m8_cpu_backend **out) {
    if(!out) return UC_ERR_ARG;
    *out=NULL;
    if(arch!=M8_CPU_BACKEND_ARCH_ARM) return UC_ERR_ARCH;
    if(mode!=M8_CPU_BACKEND_MODE_THUMB) return UC_ERR_MODE;
    m8_cpu_backend *b=calloc(1,sizeof *b);
    if(!b) return UC_ERR_NOMEM;
    b->cpu=&b->local_cpu;b->arch=&b->local_arch;b->cpsr=&b->local_cpsr;
    b->local_cpu.xpsr=0x40000000u;b->local_cpsr=0x400001d3u;
    b->local_arch.cpsr=b->local_cpsr;
    b->exclusive[0]=UINT64_MAX;
    b->generation=1;b->next_hook=1;atomic_init(&b->stop,false);
    *out=b;return UC_ERR_OK;
}
m8_cpu_backend_error m8_cpu_backend_bind_state(m8_cpu_backend *b,m8_cpu *cpu,m8_native_arch *arch,uint32_t *cpsr) {
    if(!b) return UC_ERR_HANDLE;
    if(!cpu || !arch || !cpsr || b->bound || b->map_count || b->hooks) return UC_ERR_ARG;
    b->cpu=cpu;b->arch=arch;b->cpsr=cpsr;b->bound=true;
    b->exclusive[0]=arch->exclusive_valid?arch->exclusive_address:UINT64_MAX;
    b->exclusive[1]=arch->exclusive_value;monitor_shadow(b);
    return UC_ERR_OK;
}
static void backing_release(backing *s) {
    if(s && !--s->refs) {if(s->owned) free(s->bytes);free(s);}
}
m8_cpu_backend_error m8_cpu_backend_close(m8_cpu_backend *b) {
    if(!b) return UC_ERR_HANDLE;
    for(size_t i=0;i<b->map_count;i++) backing_release(b->maps[i].storage);
    free(b->maps);
    while(b->hooks) {hook_record *next=b->hooks->next;free(b->hooks);b->hooks=next;}
    free(b);return UC_ERR_OK;
}
unsigned m8_cpu_backend_version(unsigned *major,unsigned *minor) {
    if(major) *major=1;
    if(minor) *minor=0;
    return 0x01000000u;
}
const char *m8_cpu_backend_strerror(m8_cpu_backend_error error) {
    switch(error) {
    case UC_ERR_OK:return "success";
    case UC_ERR_NOMEM:return "native backend allocation failed";
    case UC_ERR_ARCH:return "unsupported CPU architecture";
    case UC_ERR_HANDLE:return "invalid native backend handle";
    case UC_ERR_MODE:return "unsupported CPU mode";
    case UC_ERR_READ_UNMAPPED:return "unmapped memory read";
    case UC_ERR_WRITE_UNMAPPED:return "unmapped memory write";
    case UC_ERR_FETCH_UNMAPPED:return "unmapped instruction fetch";
    case UC_ERR_HOOK:return "unsupported native hook registration";
    case UC_ERR_INSN_INVALID:return "invalid instruction";
    case UC_ERR_MAP:return "invalid memory mapping";
    case UC_ERR_WRITE_PROT:return "write permission denied";
    case UC_ERR_READ_PROT:return "read permission denied";
    case UC_ERR_FETCH_PROT:return "execute permission denied";
    case UC_ERR_ARG:return "invalid argument or unsupported register";
    case UC_ERR_EXCEPTION:return "CPU execution fault";
    case M8_CPU_BACKEND_ERR_UNSUPPORTED:return "operation unavailable in native state backend";
    default:return "native backend error";
    }
}

static int general_index(int id) {
    if(id>=M8RT_REG_R0 && id<=M8RT_REG_R12) return id-M8RT_REG_R0;
    return id==M8RT_REG_SP?13:id==M8RT_REG_LR?14:id==M8RT_REG_PC?15:-1;
}
static uint32_t psr_mask(int id,bool write) {
    switch(id) {
    case M8RT_REG_APSR_NZCV:return 0xf0000000u;
    case M8RT_REG_APSR:return APSR_FLAGS|(write?0:APSR_GE);
    case M8RT_REG_APSR_NZCVQ:return APSR_FLAGS;
    case M8RT_REG_APSR_G:return APSR_GE;
    case M8RT_REG_APSR_NZCVQG:return APSR_FLAGS|APSR_GE;
    case M8RT_REG_IAPSR:return APSR_FLAGS|IPSR_BITS|(write?0:APSR_GE);
    case M8RT_REG_IAPSR_NZCVQ:return APSR_FLAGS|IPSR_BITS;
    case M8RT_REG_IAPSR_G:return APSR_GE|IPSR_BITS;
    case M8RT_REG_IAPSR_NZCVQG:return APSR_FLAGS|APSR_GE|IPSR_BITS;
    case M8RT_REG_EAPSR:return APSR_FLAGS|EPSR_BITS|(write?0:APSR_GE);
    case M8RT_REG_EAPSR_NZCVQ:return APSR_FLAGS|EPSR_BITS;
    case M8RT_REG_EAPSR_G:return APSR_GE|EPSR_BITS;
    case M8RT_REG_EAPSR_NZCVQG:return APSR_FLAGS|APSR_GE|EPSR_BITS;
    case M8RT_REG_XPSR:return APSR_FLAGS|EPSR_BITS|IPSR_BITS|(write?0:APSR_GE);
    case M8RT_REG_XPSR_NZCVQ:return APSR_FLAGS|EPSR_BITS|IPSR_BITS;
    case M8RT_REG_XPSR_G:return APSR_GE|EPSR_BITS|IPSR_BITS;
    case M8RT_REG_XPSR_NZCVQG:return APSR_FLAGS|APSR_GE|EPSR_BITS|IPSR_BITS;
    case M8RT_REG_IPSR:return IPSR_BITS;
    case M8RT_REG_EPSR:return EPSR_BITS;
    case M8RT_REG_IEPSR:return EPSR_BITS|IPSR_BITS;
    default:return 0;
    }
}
size_t m8_cpu_backend_register_size(int id) {
    if((id>=M8RT_REG_D0 && id<=M8RT_REG_D15) ||
       (id>=M8RT_REG_EXCLUSIVE_ADDR && id<=M8RT_REG_EXCLUSIVE_HIGH)) return 8;
    if(general_index(id)>=0 || (id>=M8RT_REG_S0 && id<=M8RT_REG_S31) || psr_mask(id,false)) return 4;
    switch(id) {
    case M8RT_REG_CPSR:case M8RT_REG_FPEXC:case M8RT_REG_FPSCR:case M8RT_REG_FPSCR_NZCV:
    case M8RT_REG_ITSTATE:case M8RT_REG_MSP:case M8RT_REG_PSP:case M8RT_REG_CONTROL:
    case M8RT_REG_PRIMASK:case M8RT_REG_BASEPRI:case M8RT_REG_BASEPRI_MAX:case M8RT_REG_FAULTMASK:return 4;
    default:return 0;
    }
}
m8_cpu_backend_error m8_cpu_backend_reg_read(m8_cpu_backend *b,int id,void *out) {
    if(!b) return UC_ERR_HANDLE;
    if(!out) return UC_ERR_ARG;
    if(!b->arch->ipsr && (b->arch->control&1u) && privileged_register(id)) {
        uint32_t zero=0;memcpy(out,&zero,4);return UC_ERR_OK;
    }
    /* This dispatch also validates the ID. Do not decode every register twice
     * through register_size; PC and masks are read at every service boundary. */
    uint32_t value;
    switch(id) {
    case M8RT_REG_PC:value=b->cpu->r[15];break;
    case M8RT_REG_IPSR:value=b->cpu->xpsr&IPSR_BITS;break;
    case M8RT_REG_CPSR:value=(*b->cpsr&~CPSR_SHARED)|(b->cpu->xpsr&CPSR_SHARED);break;
    case M8RT_REG_FPEXC:value=b->arch->fpexc;break;
    case M8RT_REG_FPSCR:value=b->cpu->fpscr;break;
    case M8RT_REG_FPSCR_NZCV:value=b->cpu->fpscr&0xf0000000u;break;
    case M8RT_REG_ITSTATE:value=b->arch->itstate;break;
    case M8RT_REG_MSP:value=using_psp(b->arch)?b->arch->msp:b->cpu->r[13];break;
    case M8RT_REG_PSP:value=using_psp(b->arch)?b->cpu->r[13]:b->arch->psp;break;
    case M8RT_REG_CONTROL:value=b->arch->control;break;
    case M8RT_REG_PRIMASK:value=b->arch->primask;break;
    case M8RT_REG_BASEPRI:case M8RT_REG_BASEPRI_MAX:value=b->arch->basepri;break;
    case M8RT_REG_FAULTMASK:value=b->arch->faultmask;break;
    default: {
        int n=general_index(id);
        if(n>=0) value=b->cpu->r[n];
        else if(id>=M8RT_REG_S0 && id<=M8RT_REG_S31) value=b->cpu->s[id-M8RT_REG_S0];
        else if(id>=M8RT_REG_D0 && id<=M8RT_REG_D15) {
            unsigned i=2u*(unsigned)(id-M8RT_REG_D0);
            uint64_t wide=b->cpu->s[i]|(uint64_t)b->cpu->s[i+1]<<32;
            memcpy(out,&wide,sizeof wide);return UC_ERR_OK;
        } else if(id>=M8RT_REG_EXCLUSIVE_ADDR && id<=M8RT_REG_EXCLUSIVE_HIGH) {
            monitor_sync(b);memcpy(out,&b->exclusive[id-M8RT_REG_EXCLUSIVE_ADDR],8);return UC_ERR_OK;
        } else {
            uint32_t mask=psr_mask(id,false);
            if(!mask) return UC_ERR_ARG;
            value=b->cpu->xpsr&mask;
        }
        break;
    }
    }
    memcpy(out,&value,4);return UC_ERR_OK;
}
m8_cpu_backend_error m8_cpu_backend_reg_write(m8_cpu_backend *b,int id,const void *in) {
    if(!b) return UC_ERR_HANDLE;
    if(!in || !m8_cpu_backend_register_size(id)) return UC_ERR_ARG;
    if(!b->arch->ipsr && (b->arch->control&1u) && privileged_register(id)) return UC_ERR_OK;
    if(id>=M8RT_REG_D0 && id<=M8RT_REG_D15) {
        uint64_t wide;memcpy(&wide,in,8);unsigned i=2u*(unsigned)(id-M8RT_REG_D0);
        b->cpu->s[i]=(uint32_t)wide;b->cpu->s[i+1]=(uint32_t)(wide>>32);return UC_ERR_OK;
    }
    if(id>=M8RT_REG_EXCLUSIVE_ADDR && id<=M8RT_REG_EXCLUSIVE_HIGH) {
        monitor_sync(b);memcpy(&b->exclusive[id-M8RT_REG_EXCLUSIVE_ADDR],in,8);
        b->arch->exclusive_address=(uint32_t)b->exclusive[0];
        b->arch->exclusive_value=(uint32_t)b->exclusive[1];
        b->arch->exclusive_valid=b->exclusive[0]!=UINT64_MAX;monitor_shadow(b);return UC_ERR_OK;
    }
    uint32_t value;memcpy(&value,in,4);int n=general_index(id);
    if(n>=0) {
        b->cpu->r[n]=n==15?value&~1u:value;
        if(n==13) save_sp(b);
        if(n==15) {b->cpu->xpsr=(b->cpu->xpsr&~0x01000000u)|((value&1u)<<24);publish_psr(b);}
        return UC_ERR_OK;
    }
    if(id>=M8RT_REG_S0 && id<=M8RT_REG_S31) {b->cpu->s[id-M8RT_REG_S0]=value;return UC_ERR_OK;}
    uint32_t mask=psr_mask(id,true);
    if(mask) {
        bool previous=using_psp(b->arch);save_sp(b);
        b->cpu->xpsr=(b->cpu->xpsr&~mask)|(value&mask);
        b->arch->ipsr=b->cpu->xpsr&IPSR_BITS;select_sp(b,previous);publish_psr(b);return UC_ERR_OK;
    }
    switch(id) {
    case M8RT_REG_CPSR: {
        /* The public reference API retains its old ARM mode for invalid modes.
         * Mode bits are compatibility state; execution remains Cortex-M7. */
        unsigned mode=value&31u;
        bool valid=mode==16 || mode==17 || mode==18 || mode==19 || mode==23 || mode==26 || mode==27 || mode==31;
        if(valid && mode!=19) return M8_CPU_BACKEND_ERR_UNSUPPORTED;
        *b->cpsr=(value&~31u)|(valid?mode:*b->cpsr&31u);
        b->cpu->xpsr=(b->cpu->xpsr&IPSR_BITS)|(value&CPSR_SHARED)|((value&0x20u)?0x01000000u:0);
        publish_psr(b);break;
    }
    case M8RT_REG_FPEXC:b->arch->fpexc=value;break;
    case M8RT_REG_FPSCR:b->cpu->fpscr=value&0xf7c0009fu;break;
    case M8RT_REG_FPSCR_NZCV:b->cpu->fpscr=(b->cpu->fpscr&~0xf0000000u)|(value&0xf0000000u);break;
    case M8RT_REG_ITSTATE:b->arch->itstate=value&255u;m8_native_publish_it(b->cpu,b->arch);publish_psr(b);break;
    case M8RT_REG_MSP:b->arch->msp=value;if(!using_psp(b->arch)) b->cpu->r[13]=value;break;
    case M8RT_REG_PSP:b->arch->psp=value;if(using_psp(b->arch)) b->cpu->r[13]=value;break;
    case M8RT_REG_CONTROL: {
        bool previous=using_psp(b->arch);save_sp(b);
        /* Handler mode ignores SPSEL; unprivileged Thread mode cannot promote
         * itself through this architectural control write. FPCA is privileged. */
        if(b->arch->ipsr || !(b->arch->control&1u)) {
            uint32_t allowed=b->arch->ipsr?5u:7u;
            b->arch->control=(b->arch->control&~allowed)|(value&allowed);
        }
        select_sp(b,previous);break;
    }
    case M8RT_REG_PRIMASK:b->arch->primask=value&1u;break;
    case M8RT_REG_BASEPRI:b->arch->basepri=value&255u;break;
    case M8RT_REG_BASEPRI_MAX:value&=255u;if(value && (!b->arch->basepri || value<b->arch->basepri)) b->arch->basepri=value;break;
    case M8RT_REG_FAULTMASK:b->arch->faultmask=value&1u;break;
    default:return UC_ERR_ARG;
    }
    return UC_ERR_OK;
}
m8_cpu_backend_error m8_cpu_backend_reg_read_batch(m8_cpu_backend *b,const int *ids,void *const *values,int count) {
    if(!b) return UC_ERR_HANDLE;
    if(count<0 || (count && (!ids || !values))) return UC_ERR_ARG;
    for(int i=0;i<count;i++) {m8_cpu_backend_error err=m8_cpu_backend_reg_read(b,ids[i],values[i]);if(err) return err;}
    return UC_ERR_OK;
}
m8_cpu_backend_error m8_cpu_backend_reg_write_batch(m8_cpu_backend *b,const int *ids,void *const *values,int count) {
    if(!b) return UC_ERR_HANDLE;
    if(count<0 || (count && (!ids || !values))) return UC_ERR_ARG;
    for(int i=0;i<count;i++) {m8_cpu_backend_error err=m8_cpu_backend_reg_write(b,ids[i],values[i]);if(err) return err;}
    return UC_ERR_OK;
}
m8_cpu_backend_error m8_cpu_backend_context_alloc(m8_cpu_backend *b,m8_cpu_backend_context **out) {
    if(!b) return UC_ERR_HANDLE;
    if(!out) return UC_ERR_ARG;
    *out=calloc(1,sizeof **out);return *out?UC_ERR_OK:UC_ERR_NOMEM;
}
m8_cpu_backend_error m8_cpu_backend_context_free(m8_cpu_backend_context *context) {free(context);return UC_ERR_OK;}
m8_cpu_backend_error m8_cpu_backend_context_save(m8_cpu_backend *b,m8_cpu_backend_context *context) {
    if(!b) return UC_ERR_HANDLE;
    if(!context) return UC_ERR_ARG;
    monitor_sync(b);context->cpu=*b->cpu;context->arch=*b->arch;context->cpsr=*b->cpsr;
    memcpy(context->exclusive,b->exclusive,sizeof b->exclusive);context->valid=true;return UC_ERR_OK;
}
m8_cpu_backend_error m8_cpu_backend_context_restore(m8_cpu_backend *b,const m8_cpu_backend_context *context) {
    if(!b) return UC_ERR_HANDLE;
    if(!context || !context->valid) return UC_ERR_ARG;
    *b->cpu=context->cpu;*b->arch=context->arch;*b->cpsr=context->cpsr;
    memcpy(b->exclusive,context->exclusive,sizeof b->exclusive);monitor_shadow(b);return UC_ERR_OK;
}

static bool span_valid(uint64_t address,size_t size) {return address<ADDRESS_LIMIT && (uint64_t)size<=ADDRESS_LIMIT-address;}
static bool map_valid(uint64_t address,size_t size,unsigned perms) {
    return size && span_valid(address,size) && !(address&(PAGE_SIZE-1u)) && !(size&(PAGE_SIZE-1u)) && !(perms&~7u);
}
static mapping *find_map(m8_cpu_backend *b,uint64_t address) {
    for(size_t i=0;i<b->map_count;i++) if(address>=b->maps[i].begin && address<b->maps[i].end) return &b->maps[i];
    return NULL;
}
static m8_cpu_backend_error insert_map(m8_cpu_backend *b,mapping item) {
    size_t n=b->map_count;
    for(size_t i=0;i<n;i++) if(item.begin<b->maps[i].end && item.end>b->maps[i].begin) return UC_ERR_MAP;
    if(n==SIZE_MAX/sizeof(mapping)) return UC_ERR_NOMEM;
    mapping *maps=realloc(b->maps,(n+1)*sizeof *maps);if(!maps) return UC_ERR_NOMEM;
    size_t pos=0;while(pos<n && maps[pos].begin<item.begin) pos++;
    memmove(maps+pos+1,maps+pos,(n-pos)*sizeof *maps);maps[pos]=item;
    b->maps=maps;b->map_count=n+1;b->generation++;return UC_ERR_OK;
}
static m8_cpu_backend_error map_ram(m8_cpu_backend *b,uint64_t address,size_t size,unsigned perms,void *host,bool owned) {
    if(!b) return UC_ERR_HANDLE;
    if(!map_valid(address,size,perms) || (!owned && !host)) return UC_ERR_ARG;
    backing *storage=calloc(1,sizeof *storage);if(!storage) return UC_ERR_NOMEM;
    storage->bytes=owned?calloc(1,size):host;storage->owned=owned;storage->refs=1;
    if(!storage->bytes) {free(storage);return UC_ERR_NOMEM;}
    mapping item={.begin=address,.end=address+size,.perms=perms,.host=storage->bytes,.storage=storage};
    m8_cpu_backend_error err=insert_map(b,item);if(err) backing_release(storage);return err;
}
m8_cpu_backend_error m8_cpu_backend_mem_map(m8_cpu_backend *b,uint64_t address,size_t size,unsigned perms) {return map_ram(b,address,size,perms,NULL,true);}
m8_cpu_backend_error m8_cpu_backend_mem_map_ptr(m8_cpu_backend *b,uint64_t address,size_t size,unsigned perms,void *host) {return map_ram(b,address,size,perms,host,false);}
m8_cpu_backend_error m8_cpu_backend_mmio_map(m8_cpu_backend *b,uint64_t address,size_t size,m8_cpu_backend_mmio_read_fn read,void *read_user,m8_cpu_backend_mmio_write_fn write,void *write_user) {
    if(!b) return UC_ERR_HANDLE;
    if(!map_valid(address,size,0) || (!read && !write)) return UC_ERR_ARG;
    mapping item={.begin=address,.end=address+size,.perms=(read?1u:0)|(write?2u:0),
        .read=read,.write=write,.read_user=read_user,.write_user=write_user,.mmio_base=address};
    return insert_map(b,item);
}
static m8_cpu_backend_error check_span(m8_cpu_backend *b,uint64_t address,size_t size,unsigned permission,bool host) {
    m8_cpu_backend_error unmapped=permission==2?UC_ERR_WRITE_UNMAPPED:permission==4?UC_ERR_FETCH_UNMAPPED:UC_ERR_READ_UNMAPPED;
    m8_cpu_backend_error protection=permission==2?UC_ERR_WRITE_PROT:permission==4?UC_ERR_FETCH_PROT:UC_ERR_READ_PROT;
    if(!span_valid(address,size)) return unmapped;
    uint64_t end=address+size;
    while(address<end) {
        mapping *m=find_map(b,address);if(!m || (host && !m->host)) return unmapped;
        if(!host && !(m->perms&permission)) return protection;
        if(!host && permission==4 && !m->host) return UC_ERR_FETCH_PROT;
        address=m->end<end?m->end:end;
    }
    return UC_ERR_OK;
}
static m8_cpu_backend_error host_copy(m8_cpu_backend *b,uint64_t address,void *data,size_t size,bool write) {
    if(!b) return UC_ERR_HANDLE;
    if(size && !data) return UC_ERR_ARG;
    m8_cpu_backend_error err=check_span(b,address,size,write?2:1,true);if(err) return err;
    uint8_t *bytes=data;
    while(size) {
        mapping *m=find_map(b,address);size_t part=(size_t)(m->end-address);if(part>size) part=size;
        if(write) memcpy(m->host+(size_t)(address-m->begin),bytes,part);
        else memcpy(bytes,m->host+(size_t)(address-m->begin),part);
        address+=part;bytes+=part;size-=part;
    }
    return UC_ERR_OK;
}
m8_cpu_backend_error m8_cpu_backend_mem_read(m8_cpu_backend *b,uint64_t address,void *out,size_t size) {return host_copy(b,address,out,size,false);}
m8_cpu_backend_error m8_cpu_backend_mem_write(m8_cpu_backend *b,uint64_t address,const void *in,size_t size) {return host_copy(b,address,(void *)in,size,true);}
static m8_cpu_backend_error bus_access(m8_cpu_backend *b,uint64_t address,unsigned size,uint64_t *value,bool write) {
    if(!b) return UC_ERR_HANDLE;
    if(!value || (size!=1 && size!=2 && size!=4 && size!=8)) return UC_ERR_ARG;
    m8_cpu_backend_error err=check_span(b,address,size,write?2:1,false);if(err) return err;
    mapping *m=find_map(b,address);
    if(!m->host) {
        if(address+size>m->end) return write?UC_ERR_WRITE_UNMAPPED:UC_ERR_READ_UNMAPPED;
        if(write?!m->write:!m->read) return write?UC_ERR_WRITE_PROT:UC_ERR_READ_PROT;
        uint64_t mask=size==8?UINT64_MAX:(UINT64_C(1)<<(8*size))-1u;
        if(write) m->write(b,address-m->mmio_base,size,*value&mask,m->write_user);
        else *value=m->read(b,address-m->mmio_base,size,m->read_user)&mask;
        return UC_ERR_OK;
    }
    /* Reject a RAM-to-MMIO span before copying any prefix or invoking a device. */
    err=check_span(b,address,size,write?2:1,true);if(err) return err;
    uint8_t bytes[8];
    if(write) {for(unsigned i=0;i<size;i++) bytes[i]=(uint8_t)(*value>>(8*i));return host_copy(b,address,bytes,size,true);}
    err=host_copy(b,address,bytes,size,false);
    if(!err) {*value=0;for(unsigned i=0;i<size;i++) *value|=(uint64_t)bytes[i]<<(8*i);}
    return err;
}
m8_cpu_backend_error m8_cpu_backend_bus_read(m8_cpu_backend *b,uint64_t address,unsigned size,uint64_t *out) {return bus_access(b,address,size,out,false);}
m8_cpu_backend_error m8_cpu_backend_bus_write(m8_cpu_backend *b,uint64_t address,unsigned size,uint64_t value) {return bus_access(b,address,size,&value,true);}
m8_cpu_backend_error m8_cpu_backend_fetch(m8_cpu_backend *b,uint64_t address,void *out,size_t size) {
    if(!b) return UC_ERR_HANDLE;
    if(size && !out) return UC_ERR_ARG;
    m8_cpu_backend_error err=check_span(b,address,size,4,false);if(err) return err;
    return host_copy(b,address,out,size,false);
}
static void append_piece(mapping *maps,size_t *used,mapping old,uint64_t begin,uint64_t end,unsigned perms) {
    if(begin==end) return;
    if(old.host) old.host+=(size_t)(begin-old.begin);
    old.begin=begin;old.end=end;old.perms=perms;
    if(old.storage) old.storage->refs++;
    maps[(*used)++]=old;
}
static m8_cpu_backend_error change_maps(m8_cpu_backend *b,uint64_t address,size_t size,unsigned perms,bool unmap) {
    if(!b) return UC_ERR_HANDLE;
    if(!map_valid(address,size,perms)) return UC_ERR_ARG;
    uint64_t end=address+size,cursor=address;
    while(cursor<end) {mapping *m=find_map(b,cursor);if(!m) return UC_ERR_MAP;cursor=m->end<end?m->end:end;}
    if(b->map_count>SIZE_MAX/(3*sizeof(mapping))) return UC_ERR_NOMEM;
    mapping *maps=malloc(3*b->map_count*sizeof *maps);if(!maps) return UC_ERR_NOMEM;
    size_t used=0;
    for(size_t i=0;i<b->map_count;i++) {
        mapping m=b->maps[i];
        if(m.end<=address || m.begin>=end) append_piece(maps,&used,m,m.begin,m.end,m.perms);
        else {
            uint64_t from=m.begin>address?m.begin:address,to=m.end<end?m.end:end;
            append_piece(maps,&used,m,m.begin,from,m.perms);
            if(!unmap) append_piece(maps,&used,m,from,to,perms);
            append_piece(maps,&used,m,to,m.end,m.perms);
        }
    }
    for(size_t i=0;i<b->map_count;i++) backing_release(b->maps[i].storage);
    free(b->maps);b->maps=maps;b->map_count=used;b->generation++;return UC_ERR_OK;
}
m8_cpu_backend_error m8_cpu_backend_mem_unmap(m8_cpu_backend *b,uint64_t address,size_t size) {return change_maps(b,address,size,0,true);}
m8_cpu_backend_error m8_cpu_backend_mem_protect(m8_cpu_backend *b,uint64_t address,size_t size,unsigned perms) {return change_maps(b,address,size,perms,false);}
const uint64_t *m8_cpu_backend_memory_generation(m8_cpu_backend *b) {return b?&b->generation:NULL;}

m8_cpu_backend_error m8_cpu_backend_hook_add(m8_cpu_backend *b,m8_cpu_backend_hook *out,int type,void *callback,void *user,uint64_t begin,uint64_t end,...) {
    if(!b) return UC_ERR_HANDLE;
    if(!out || !callback) return UC_ERR_ARG;
    *out=0;
    const int supported=M8_CPU_BACKEND_HOOK_INTR|M8_CPU_BACKEND_HOOK_CODE|M8_CPU_BACKEND_HOOK_BLOCK|
        M8_CPU_BACKEND_HOOK_MEM_UNMAPPED|M8_CPU_BACKEND_HOOK_MEM_PROT|M8_CPU_BACKEND_HOOK_MEM_WRITE;
    if(!type || (type&~supported)) return UC_ERR_HOOK;
    hook_record *h=malloc(sizeof *h);if(!h) return UC_ERR_NOMEM;
    if(!b->next_hook) {free(h);return UC_ERR_RESOURCE;}
    *h=(hook_record){.id=b->next_hook++,.type=type,.callback=callback,.user=user,.begin=begin,.end=end,.next=b->hooks};
    b->hooks=h;*out=h->id;return UC_ERR_OK;
}
m8_cpu_backend_error m8_cpu_backend_hook_del(m8_cpu_backend *b,m8_cpu_backend_hook id) {
    if(!b) return UC_ERR_HANDLE;
    hook_record **p=&b->hooks;
    while(*p && (*p)->id!=id) p=&(*p)->next;
    if(!*p) return UC_ERR_HOOK;
    hook_record *h=*p;*p=h->next;free(h);return UC_ERR_OK;
}
m8_cpu_backend_error m8_cpu_backend_ctl_set_cpu_model(m8_cpu_backend *b,int model) {return !b?UC_ERR_HANDLE:model==M8_CPU_BACKEND_CPU_CORTEX_M7?UC_ERR_OK:UC_ERR_ARG;}
m8_cpu_backend_error m8_cpu_backend_ctl_set_tcg_buffer_size(m8_cpu_backend *b,uint32_t size) {(void)size;return b?M8_CPU_BACKEND_ERR_UNSUPPORTED:UC_ERR_HANDLE;}
m8_cpu_backend_error m8_cpu_backend_ctl_flush_tb(m8_cpu_backend *b) {return b?UC_ERR_OK:UC_ERR_HANDLE;}
m8_cpu_backend_error m8_cpu_backend_ctl_remove_cache(m8_cpu_backend *b,uint64_t begin,uint64_t end) {
    if(!b) return UC_ERR_HANDLE;
    return begin<=end && end<=ADDRESS_LIMIT?UC_ERR_OK:UC_ERR_ARG;
}
unsigned m8_cpu_backend_arm_exclusive_monitor_abi(void) {return 1;}
m8_cpu_backend_error m8_cpu_backend_emu_stop(m8_cpu_backend *b) {
    if(!b) return UC_ERR_HANDLE;
    atomic_store_explicit(&b->stop,true,memory_order_release);return UC_ERR_OK;
}
bool m8_cpu_backend_stop_requested(const m8_cpu_backend *b) {return b && atomic_load_explicit(&b->stop,memory_order_acquire);}
void m8_cpu_backend_clear_stop(m8_cpu_backend *b) {if(b) atomic_store_explicit(&b->stop,false,memory_order_release);}
m8_cpu_backend_error m8_cpu_backend_emu_start(m8_cpu_backend *b,uint64_t begin,uint64_t until,uint64_t timeout,size_t count) {
    (void)begin;(void)until;(void)timeout;(void)count;
    return b?M8_CPU_BACKEND_ERR_UNSUPPORTED:UC_ERR_HANDLE;
}

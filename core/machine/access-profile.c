#define _GNU_SOURCE
#include "internal.h"
#include <dlfcn.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#define ACCESS_CAPACITY 8192u
typedef struct {
    uint32_t address,size,path,write;
    uint64_t count;
} access_row;
typedef struct {
    uint32_t address,size,write;
    uintptr_t caller;
    uint64_t count;
} site_row;
struct m8rt_access_profile {
    bool enabled, counts, storage_wait;
    uint64_t storage_exports,storage_imports,storage_export_ns,storage_exec_ns,storage_import_ns;
    uint64_t storage_phase_start,storage_read_bytes;
    uint32_t storage_guard_address,storage_guard_irq_nest,storage_guard_pc;
    bool storage_guard_read;
    size_t used;
    uint64_t first_cycles,first_frames;
    uint64_t nvic[4];
    access_row rows[ACCESS_CAPACITY];
    site_row sites[ACCESS_CAPACITY];
};
int m8rt_set_access_profile(m8rt *rt,int enabled) {
    if(!rt || rt->in_emu || (enabled!=0 && enabled!=1)) return -1;
    if(enabled) {
        if(!rt->access_profile) rt->access_profile=calloc(1,sizeof(*rt->access_profile));
        if(!rt->access_profile) return -1;
        memset(rt->access_profile,0,sizeof(*rt->access_profile));
        rt->access_profile->counts=!(getenv("M8RT_ACCESS_COUNTS") && !strcmp(getenv("M8RT_ACCESS_COUNTS"),"0"));
        rt->access_profile->first_cycles=rt->guest_cycles;
        rt->access_profile->first_frames=rt->local_total/4;
    }
    if(rt->access_profile) rt->access_profile->enabled=enabled;
    return 0;
}
void m8rt_access_record(m8rt *rt,uint32_t address,unsigned size,unsigned path,unsigned write) {
    m8rt_access_profile *p=rt->access_profile;
    if(!p || !p->enabled || !p->counts) return;
    size_t slot=((address>>1)*UINT32_C(2654435761)+size*17+path*7+write)&(ACCESS_CAPACITY-1);
    for(unsigned probes=0;probes<ACCESS_CAPACITY;probes++,slot=(slot+1)&(ACCESS_CAPACITY-1)) {
        access_row *r=&p->rows[slot];
        if(!r->count) {r->address=address;r->size=size;r->path=path;r->write=write;p->used++;}
        if(r->address==address && r->size==size && r->path==path && r->write==write) {r->count++;return;}
    }
    m8rt_fail(rt,"slow access profile capacity address=%08x size=%u path=%u write=%u cap=%u",address,size,path,write,ACCESS_CAPACITY);
}
/* Count only at the native callback entry. Return-address provenance avoids
 * reading a stale guest PC while an AOT function keeps registers in locals.
 * Symbol resolution happens after playback, never inside the measured loop. */
void m8rt_access_site(m8rt *rt,uint32_t address,unsigned size,unsigned write,uintptr_t caller) {
    m8rt_access_profile *p=rt->access_profile;
    if(!p || !p->enabled || !p->counts) return;
    size_t slot=((address>>1)*UINT32_C(2654435761)+(caller>>2)+size*17+write)&(ACCESS_CAPACITY-1);
    for(unsigned probes=0;probes<ACCESS_CAPACITY;probes++,slot=(slot+1)&(ACCESS_CAPACITY-1)) {
        site_row *r=&p->sites[slot];
        if(!r->count) {r->address=address;r->size=size;r->write=write;r->caller=caller;}
        if(r->address==address && r->size==size && r->write==write && r->caller==caller) {r->count++;return;}
    }
    m8rt_fail(rt,"native device site profile capacity address=%08x size=%u write=%u caller=%" PRIxPTR " cap=%u",address,size,write,caller,ACCESS_CAPACITY);
}
/* Only storage-triggered live transitions are timed. An unfinished reference
 * interval remains open in the report and is not counted as a native import. */
void m8rt_access_storage_phase(m8rt *rt,unsigned phase) {
    m8rt_access_profile *p=rt->access_profile;
    if(!p || !p->enabled) return;
    if(phase==0) p->storage_phase_start=m8rt_now_ns();
    else if(phase==1) {
        p->storage_export_ns+=m8rt_now_ns()-p->storage_phase_start;
        p->storage_exports++;p->storage_wait=true;
    } else if(p->storage_wait && phase==2) p->storage_phase_start=m8rt_now_ns();
    else if(p->storage_wait && phase==3) {
        p->storage_import_ns+=m8rt_now_ns()-p->storage_phase_start;
        p->storage_imports++;p->storage_wait=false;
    }
}
void m8rt_access_storage_exec(m8rt *rt,uint64_t ns) {
    m8rt_access_profile *p=rt->access_profile;
    if(p && p->enabled && p->storage_wait) p->storage_exec_ns+=ns;
}
void m8rt_access_storage_read(m8rt *rt,unsigned bytes) {
    m8rt_access_profile *p=rt->access_profile;
    if(p && p->enabled) p->storage_read_bytes+=bytes;
}
void m8rt_access_storage_guard(m8rt *rt,uint32_t addr,bool read) {
    m8rt_access_profile *p=rt->access_profile;
    if(!p || !p->enabled || p->storage_guard_address) return;
    p->storage_guard_address=addr;p->storage_guard_irq_nest=rt->irq_nest;
#ifdef M8RT_NATIVE_ENGINE
    p->storage_guard_pc=rt->native.cpu.r[15];
#endif
    p->storage_guard_read=read;
}
static int compare(const void *a,const void *b) {
    uint64_t x=((const access_row *)a)->count,y=((const access_row *)b)->count;
    return x<y?1:x>y?-1:0;
}
void m8rt_access_nvic(m8rt *rt,unsigned stage) {
    if(rt->access_profile && rt->access_profile->enabled && rt->access_profile->counts && stage<4)
        rt->access_profile->nvic[stage]++;
}
int m8rt_dump_access_profile(m8rt *rt,const char *path) {
    if(!rt || rt->in_emu || !rt->access_profile || rt->access_profile->enabled) return -1;
    m8rt_access_profile *p=rt->access_profile;
    access_row *rows=calloc(p->used?p->used:1,sizeof(*rows));
    if(!rows) return -1;
    size_t n=0;for(size_t i=0;i<ACCESS_CAPACITY;i++) if(p->rows[i].count) rows[n++]=p->rows[i];
    qsort(rows,n,sizeof(*rows),compare);
    FILE *f=fopen(path,"w");if(!f) {free(rows);return -1;}
    fprintf(f,"{\"schema\":\"m8-access/1\",\"audio_frames\":%" PRIu64 ",\"guest_cycles\":%" PRIu64 ",\"end_iters\":%" PRIu64 ",\"end_pc\":%" PRIu64 ",\"rows\":[",rt->local_total/4-p->first_frames,rt->guest_cycles-p->first_cycles,rt->main_iters,m8rt_get(rt,M8RT_PC));
    for(size_t i=0;i<n;i++) fprintf(f,"%s{\"address\":%u,\"size\":%u,\"caller\":\"%s\",\"direction\":\"%s\",\"count\":%" PRIu64 "}",i?",":"",rows[i].address,rows[i].size,rows[i].path==1?"TCG":"AOT",rows[i].write?"write":"read",rows[i].count);
    fprintf(f,"],\"nvic_result\":{\"calls\":%" PRIu64 ",\"admitted\":%" PRIu64 ",\"unmasked_thread\":%" PRIu64 ",\"banks_tested\":%" PRIu64 "},\"native_device_sites\":[",p->nvic[0],p->nvic[1],p->nvic[2],p->nvic[3]);
    bool comma=false;
    for(size_t i=0;i<ACCESS_CAPACITY;i++) if(p->sites[i].count) {
        site_row *r=&p->sites[i];Dl_info info={0};
        uintptr_t call=r->caller?r->caller-1:0;
        bool found=dladdr((void *)call,&info)!=0;
        uintptr_t offset=found?call-(uintptr_t)info.dli_fbase:call;
        /* ELF filenames normally contain neither quote nor backslash. Fail
         * explicitly rather than write a malformed profile for such a path. */
        const char *object=found?info.dli_fname:"";
        if(strpbrk(object,"\"\\\n\r")) {fclose(f);free(rows);return -1;}
        fprintf(f,"%s{\"address\":%u,\"size\":%u,\"direction\":\"%s\",\"count\":%" PRIu64 ",\"object\":\"%s\",\"host_offset\":%" PRIuPTR ",\"resolved\":%s}",comma?",":"",r->address,r->size,r->write?"write":"read",r->count,object,offset,found?"true":"false");
        comma=true;
    }
    fprintf(f,"],\"storage_handoffs\":{\"exports\":%" PRIu64 ",\"imports\":%" PRIu64
        ",\"export_ns\":%" PRIu64 ",\"reference_exec_ns\":%" PRIu64 ",\"import_ns\":%" PRIu64
        ",\"reference_open\":%s,\"read_bytes\":%" PRIu64 ",\"counts_enabled\":%s}",
        p->storage_exports,p->storage_imports,p->storage_export_ns,p->storage_exec_ns,p->storage_import_ns,
        p->storage_wait?"true":"false",p->storage_read_bytes,p->counts?"true":"false");
    fprintf(f,",\"first_storage_guard\":{\"address\":%u,\"irq_nest\":%u,\"entry_pc\":%u,\"read\":%s}}\n",
        p->storage_guard_address,p->storage_guard_irq_nest,p->storage_guard_pc,
        p->storage_guard_read?"true":"false");
    int bad=ferror(f);if(fclose(f)) bad=1;free(rows);return bad?-1:0;
}
void m8rt_access_profile_destroy(m8rt *rt) {free(rt->access_profile);rt->access_profile=NULL;}

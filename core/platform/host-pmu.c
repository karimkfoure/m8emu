#define _GNU_SOURCE
#include "host-pmu.h"
#include "pc-profile.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int parse_sample(const char *spec, unsigned *event, uint64_t *period) {
    *event=0;*period=0;
    if(!spec) return 0;
    if(!*spec) { errno=EINVAL;return -1; }
    const char *colon=strchr(spec,':');
    if(!colon || !colon[1]) { errno=EINVAL;return -1; }
    size_t length=(size_t)(colon-spec);
    if(length==6 && !memcmp(spec,"cycles",6)) *event=1;
    else if(length==10 && !memcmp(spec,"l1i-refill",10)) *event=2;
    else if(length==10 && !memcmp(spec,"l2d-refill",10)) *event=3;
    else { errno=EINVAL;return -1; }
    uint64_t value=0;
    for(const char *p=colon+1;*p;p++) {
        if(*p<'0' || *p>'9' || value>(UINT64_MAX-(unsigned)(*p-'0'))/10) {
            errno=EINVAL;return -1;
        }
        value=value*10+(unsigned)(*p-'0');
    }
    if(!value) { errno=EINVAL;return -1; }
    *period=value;return 0;
}

#if defined(__linux__) && (defined(__x86_64__) || defined(__aarch64__))
#include <dirent.h>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

/* Linux 6.0 added this read-format bit; its ABI is usable with older headers.
 * The existing EINVAL retry below handles kernels without the extension. */
#define M8_PERF_FORMAT_LOST (UINT64_C(1) << 4)

enum { CYCLES, INSTRUCTIONS, L1I_ACCESS, L1I_REFILL, L1D_ACCESS, L1D_REFILL,
    L2D_ACCESS, L2D_REFILL, ITLB_REFILL, DTLB_REFILL, BRANCH_MISS,
    FRONTEND_STALL, BACKEND_STALL, LL_ACCESS, LL_REFILL, EVENT_COUNT };
enum { DATA_PAGES=256, SAMPLE_CAPACITY=65536 };
typedef struct {
    const char *name, *source, *support_source, *stage;
    uint32_t type;
    uint64_t config, raw, enabled, running;
    int fd, error, read_ok, started, advertised;
} counter;
typedef struct {
    counter event;
    unsigned requested;
    uint64_t period, lost_records, lost_samples_records, lost_read, dropped;
    uint64_t other_records, malformed_records, throttle_records, unthrottle_records;
    int lost_read_supported, lost_read_ok, ring_saturated, truncated;
    void *mapping;
    size_t map_size, data_offset, data_size, count;
    uintptr_t *pcs;
} sampler;
static counter events[EVENT_COUNT];
static sampler samples;
static int prepared, counting, started, stopped;
static uint32_t arm_type;
static int arm_type_error;
static char arm_name[128];
static const char *const event_names[EVENT_COUNT]={
    "cycles","instructions","l1i-access","l1i-refill","l1d-access","l1d-refill",
    "l2d-access","l2d-refill","itlb-refill","dtlb-refill","branch-miss",
    "frontend-stall","backend-stall","ll-access","ll-refill"
};
#if defined(__aarch64__)
static const uint64_t arm_events[EVENT_COUNT]={
    0x11,0x08,0x14,0x01,0x04,0x03,0x16,0x17,0x02,0x05,0x10,0x23,0x24,0,0
};
static const char *const arm_aliases[EVENT_COUNT]={
    "cpu_cycles","inst_retired","l1i_cache","l1i_cache_refill","l1d_cache","l1d_cache_refill",
    "l2d_cache","l2d_cache_refill","l1i_tlb_refill","l1d_tlb_refill","br_mis_pred",
    "stall_frontend","stall_backend",NULL,NULL
};
#endif

static void fail(counter *event,const char *stage,int error) {
    if(!event->error) { event->stage=stage;event->error=error?error:EIO; }
}
static int perf_open(struct perf_event_attr *attr) {
    return (int)syscall(SYS_perf_event_open,attr,0,-1,-1,PERF_FLAG_FD_CLOEXEC);
}
#if defined(__x86_64__)
static uint64_t cache_event(unsigned cache,unsigned result) {
    return cache | ((uint64_t)PERF_COUNT_HW_CACHE_OP_READ<<8) | ((uint64_t)result<<16);
}
#endif
static void discover_arm_type(void) {
    arm_type=PERF_TYPE_RAW;arm_type_error=ENOENT;arm_name[0]=0;
#if defined(__aarch64__)
    DIR *directory=opendir("/sys/bus/event_source/devices");
    if(!directory) { arm_type_error=errno;return; }
    struct dirent *entry;
    while((entry=readdir(directory))) {
        if(strncmp(entry->d_name,"armv8",5)) continue;
        char path[512];
        int length=snprintf(path,sizeof path,"/sys/bus/event_source/devices/%s/type",entry->d_name);
        if(length<0 || (size_t)length>=sizeof path) continue;
        FILE *input=fopen(path,"r");unsigned type;
        if(!input) { arm_type_error=errno;continue; }
        int ok=fscanf(input,"%u",&type)==1;fclose(input);
        if(!ok) { arm_type_error=EINVAL;continue; }
        arm_type=type;arm_type_error=0;
        snprintf(arm_name,sizeof arm_name,"%.127s",entry->d_name);
        break;
    }
    closedir(directory);
#endif
}
static void configure_event(counter *event,unsigned index) {
    memset(event,0,sizeof *event);event->fd=-1;event->advertised=-1;
    event->name=event_names[index];
    event->support_source="kernel_event_open";
    event->type=PERF_TYPE_HARDWARE;event->source="linux-generic-hardware";
    if(index==CYCLES) { event->config=PERF_COUNT_HW_CPU_CYCLES;return; }
    if(index==INSTRUCTIONS) { event->config=PERF_COUNT_HW_INSTRUCTIONS;return; }
#if defined(__aarch64__)
    event->type=arm_type;event->config=arm_events[index];event->source="armv8-architectural-raw";
    if(index==LL_ACCESS || index==LL_REFILL) { fail(event,"generic_ll_unavailable",ENOTSUP);return; }
    if(arm_type_error) { fail(event,"pmu_sysfs_type",arm_type_error);return; }
    event->support_source="sysfs_pmceid_filtered_alias";
    char path[512];
    snprintf(path,sizeof path,"/sys/bus/event_source/devices/%s/events/%s",arm_name,arm_aliases[index]);
    if(access(path,R_OK)==0) event->advertised=1;
    else {
        int error=errno;event->advertised=0;
        /* Raw open can accept unsupported selectors and return silent zero.
         * Linux filters these architectural aliases through PMCEID support. */
        fail(event,"sysfs_event_alias",error==ENOENT?ENOTSUP:error);
    }
#else
    event->type=PERF_TYPE_HW_CACHE;event->source="linux-generic-read-cache";
    switch(index) {
    case L1I_ACCESS:event->config=cache_event(PERF_COUNT_HW_CACHE_L1I,PERF_COUNT_HW_CACHE_RESULT_ACCESS);break;
    case L1I_REFILL:event->config=cache_event(PERF_COUNT_HW_CACHE_L1I,PERF_COUNT_HW_CACHE_RESULT_MISS);break;
    case L1D_ACCESS:event->config=cache_event(PERF_COUNT_HW_CACHE_L1D,PERF_COUNT_HW_CACHE_RESULT_ACCESS);break;
    case L1D_REFILL:event->config=cache_event(PERF_COUNT_HW_CACHE_L1D,PERF_COUNT_HW_CACHE_RESULT_MISS);break;
    case LL_ACCESS:event->config=cache_event(PERF_COUNT_HW_CACHE_LL,PERF_COUNT_HW_CACHE_RESULT_ACCESS);break;
    case LL_REFILL:event->config=cache_event(PERF_COUNT_HW_CACHE_LL,PERF_COUNT_HW_CACHE_RESULT_MISS);break;
    case ITLB_REFILL:event->config=cache_event(PERF_COUNT_HW_CACHE_ITLB,PERF_COUNT_HW_CACHE_RESULT_MISS);break;
    case DTLB_REFILL:event->config=cache_event(PERF_COUNT_HW_CACHE_DTLB,PERF_COUNT_HW_CACHE_RESULT_MISS);break;
    case BRANCH_MISS:event->type=PERF_TYPE_HARDWARE;event->source="linux-generic-hardware";event->config=PERF_COUNT_HW_BRANCH_MISSES;break;
    case FRONTEND_STALL:event->type=PERF_TYPE_HARDWARE;event->source="linux-generic-hardware";event->config=PERF_COUNT_HW_STALLED_CYCLES_FRONTEND;break;
    case BACKEND_STALL:event->type=PERF_TYPE_HARDWARE;event->source="linux-generic-hardware";event->config=PERF_COUNT_HW_STALLED_CYCLES_BACKEND;break;
    default:event->source="no-generic-exact-l2";fail(event,"generic_l2_unavailable",ENOTSUP);break;
    }
#endif
}
static struct perf_event_attr attributes(const counter *event) {
    struct perf_event_attr attr={0};attr.size=sizeof attr;
    attr.type=event->type;attr.config=event->config;attr.disabled=1;
    attr.exclude_kernel=1;attr.exclude_hv=1;
    attr.read_format=PERF_FORMAT_TOTAL_TIME_ENABLED|PERF_FORMAT_TOTAL_TIME_RUNNING;
    return attr;
}
static int valid(const counter *event) {
    return event->read_ok && !event->error && event->enabled && event->running && event->running<=event->enabled;
}
static long double scaled(const counter *event) {
    return (long double)event->raw*event->enabled/event->running;
}
static void read_counter(counter *event,int include_lost,uint64_t *lost) {
    uint64_t data[4]={0};size_t length=include_lost?sizeof data:3*sizeof *data;
    ssize_t result;
    do result=read(event->fd,data,length);while(result<0 && errno==EINTR);
    if(result!=(ssize_t)length) { fail(event,"read",result<0?errno:EIO);return; }
    event->raw=data[0];event->enabled=data[1];event->running=data[2];event->read_ok=1;
    if(include_lost && lost) *lost=data[3];
    if(!event->enabled || !event->running) fail(event,"not_scheduled",EAGAIN);
    else if(event->running>event->enabled) fail(event,"invalid_running_time",EIO);
}
static void ring_copy(const sampler *sample,uint64_t position,void *output,size_t size) {
    size_t offset=(size_t)(position&(sample->data_size-1));
    size_t first=sample->data_size-offset;if(first>size) first=size;
    const uint8_t *data=(const uint8_t *)sample->mapping+sample->data_offset;
    memcpy(output,data+offset,first);
    if(first<size) memcpy((uint8_t *)output+first,data,size-first);
}
static void add_saturated(uint64_t *value,uint64_t increment) {
    *value=UINT64_MAX-*value<increment?UINT64_MAX:*value+increment;
}
static void consume_ring(sampler *sample) {
    if(!sample->mapping) return;
    struct perf_event_mmap_page *metadata=sample->mapping;
    uint64_t head=__atomic_load_n(&metadata->data_head,__ATOMIC_ACQUIRE);
    uint64_t tail=__atomic_load_n(&metadata->data_tail,__ATOMIC_RELAXED);
    if(head<tail || head-tail>sample->data_size) {
        fail(&sample->event,"ring_overrun",EOVERFLOW);sample->truncated=1;tail=head;
    }
    if(head-tail>=sample->data_size-sizeof(struct perf_event_header)-sizeof(uint64_t)) sample->ring_saturated=1;
    while(tail<head) {
        struct perf_event_header header;
        if(head-tail<sizeof header) goto malformed;
        ring_copy(sample,tail,&header,sizeof header);
        if(header.size<sizeof header || header.size>sample->data_size || header.size>head-tail) goto malformed;
        if(header.type==PERF_RECORD_SAMPLE) {
            uint64_t ip;
            if(header.size<sizeof header+sizeof ip) goto malformed;
            ring_copy(sample,tail+sizeof header,&ip,sizeof ip);
            if(sample->count<SAMPLE_CAPACITY) sample->pcs[sample->count++]=(uintptr_t)ip;
            else { add_saturated(&sample->dropped,1);sample->truncated=1; }
        } else if(header.type==PERF_RECORD_LOST) {
            uint64_t lost;
            if(header.size<sizeof header+2*sizeof lost) goto malformed;
            ring_copy(sample,tail+sizeof header+sizeof(uint64_t),&lost,sizeof lost);
            add_saturated(&sample->lost_records,lost);
        } else if(header.type==PERF_RECORD_LOST_SAMPLES) {
            uint64_t lost;
            if(header.size<sizeof header+sizeof lost) goto malformed;
            ring_copy(sample,tail+sizeof header,&lost,sizeof lost);
            add_saturated(&sample->lost_samples_records,lost);
        } else if(header.type==PERF_RECORD_THROTTLE || header.type==PERF_RECORD_UNTHROTTLE) {
            if(header.size<sizeof header+3*sizeof(uint64_t)) goto malformed;
            if(header.type==PERF_RECORD_THROTTLE) { sample->throttle_records++;sample->truncated=1; }
            else sample->unthrottle_records++;
        } else sample->other_records++;
        tail+=header.size;continue;
malformed:
        sample->malformed_records++;sample->truncated=1;
        fail(&sample->event,"ring_parse",EBADMSG);tail=head;
    }
    __atomic_store_n(&metadata->data_tail,tail,__ATOMIC_RELEASE);
}
static void prepare_sampling(unsigned requested,uint64_t period) {
    samples.requested=requested;samples.period=period;
    unsigned index=requested==1?CYCLES:requested==2?L1I_REFILL:L2D_REFILL;
#if defined(__x86_64__)
    if(index==L2D_REFILL) index=LL_REFILL;
#endif
    configure_event(&samples.event,index);
    if(samples.event.error) return;
    struct perf_event_attr attr=attributes(&samples.event);
    attr.sample_period=period;attr.sample_type=PERF_SAMPLE_IP;attr.wakeup_events=1;
    attr.read_format|=M8_PERF_FORMAT_LOST;samples.lost_read_supported=1;
    samples.event.fd=perf_open(&attr);
    if(samples.event.fd<0 && errno==EINVAL) {
        /* PERF_FORMAT_LOST needs Linux 6.0. Older kernels can still sample,
         * but an undrained full ring can hide its final lost-record count. */
        attr.read_format&=~M8_PERF_FORMAT_LOST;samples.lost_read_supported=0;
        samples.event.fd=perf_open(&attr);
    }
    if(samples.event.fd<0) { fail(&samples.event,"open",errno);return; }
    long page=sysconf(_SC_PAGESIZE);
    if(page<=0) { fail(&samples.event,"page_size",EINVAL);return; }
    samples.map_size=(DATA_PAGES+1)*(size_t)page;
    samples.mapping=mmap(NULL,samples.map_size,PROT_READ|PROT_WRITE,MAP_SHARED,samples.event.fd,0);
    if(samples.mapping==MAP_FAILED) { samples.mapping=NULL;fail(&samples.event,"mmap",errno);return; }
    struct perf_event_mmap_page *metadata=samples.mapping;
    samples.data_offset=metadata->data_offset?(size_t)metadata->data_offset:(size_t)page;
    samples.data_size=metadata->data_size?(size_t)metadata->data_size:DATA_PAGES*(size_t)page;
    if(!samples.data_size || (samples.data_size&(samples.data_size-1)) ||
       samples.data_offset<sizeof *metadata || samples.data_offset>samples.map_size ||
       samples.data_size>samples.map_size-samples.data_offset) {
        fail(&samples.event,"mmap_layout",EINVAL);return;
    }
    samples.pcs=calloc(SAMPLE_CAPACITY,sizeof *samples.pcs);
    if(!samples.pcs) { fail(&samples.event,"sample_buffer",ENOMEM);return; }
    /* Fault in both buffers before playback. No allocations or polling thread
     * run during measurement; a full non-overwrite ring reports loss. */
    for(size_t i=0;i<SAMPLE_CAPACITY;i+=(size_t)page/sizeof *samples.pcs) ((volatile uintptr_t *)samples.pcs)[i]=0;
    for(size_t i=0;i<samples.map_size;i+=(size_t)page) (void)((volatile uint8_t *)samples.mapping)[i];
}

int m8_pmu_prepare(int count,const char *sample_spec) {
    unsigned requested;uint64_t period;
    if(count<0 || parse_sample(sample_spec,&requested,&period)) { errno=EINVAL;return -1; }
    m8_pmu_destroy();memset(events,0,sizeof events);memset(&samples,0,sizeof samples);
    samples.event.fd=-1;counting=count!=0;
    if(!counting && !requested) {
        for(unsigned i=0;i<EVENT_COUNT;i++) events[i].fd=-1;
        prepared=1;return 0;
    }
    discover_arm_type();
    for(unsigned i=0;i<EVENT_COUNT;i++) {
        configure_event(&events[i],i);
        if(!counting || events[i].error) continue;
        struct perf_event_attr attr=attributes(&events[i]);
        events[i].fd=perf_open(&attr);
        if(events[i].fd<0) fail(&events[i],"open",errno);
    }
    if(requested) prepare_sampling(requested,period);
    prepared=1;return 0;
}
static void start_counter(counter *event) {
    if(event->fd<0 || event->error) return;
    if(ioctl(event->fd,PERF_EVENT_IOC_RESET,0)) { fail(event,"reset",errno);return; }
    if(ioctl(event->fd,PERF_EVENT_IOC_ENABLE,0)) { fail(event,"enable",errno);return; }
    event->started=1;
}
int m8_pmu_start(void) {
    if(!prepared || started || stopped) { errno=EINVAL;return -1; }
    for(unsigned i=0;i<EVENT_COUNT;i++) if(counting) start_counter(&events[i]);
    if(samples.requested && samples.mapping && samples.pcs && !samples.event.error) {
        struct perf_event_mmap_page *metadata=samples.mapping;
        uint64_t head=__atomic_load_n(&metadata->data_head,__ATOMIC_ACQUIRE);
        __atomic_store_n(&metadata->data_tail,head,__ATOMIC_RELEASE);
        start_counter(&samples.event);
    }
    started=1;return 0;
}
static void disable_counter(counter *event) {
    if(!event->started) return;
    if(ioctl(event->fd,PERF_EVENT_IOC_DISABLE,0)) fail(event,"disable",errno);
}
void m8_pmu_stop(void) {
    if(!started || stopped) return;
    /* Disable all events before reading or classifying any data. */
    disable_counter(&samples.event);
    for(unsigned i=0;i<EVENT_COUNT;i++) if(counting) disable_counter(&events[i]);
    if(samples.event.started) {
        read_counter(&samples.event,samples.lost_read_supported,&samples.lost_read);
        samples.event.started=0;
        samples.lost_read_ok=samples.event.read_ok && samples.lost_read_supported;
    }
    for(unsigned i=0;i<EVENT_COUNT;i++) if(counting && events[i].started) {
        read_counter(&events[i],0,NULL);events[i].started=0;
    }
    if(samples.mapping && samples.pcs) consume_ring(&samples);
    started=0;stopped=1;
}
int m8_pmu_instructions(uint64_t *value) {
    if(!value) { errno=EINVAL;return -1; }
    counter *event=&events[INSTRUCTIONS];
    if(!counting || !valid(event)) { errno=event->error?event->error:ENODATA;return -1; }
    long double result=scaled(event);
    if(result>(long double)UINT64_MAX) { errno=ERANGE;return -1; }
    if(result>=(long double)UINT64_MAX-0.5L) *value=UINT64_MAX;
    else *value=(uint64_t)(result+0.5L);
    return 0;
}
static void json_string(FILE *out,const char *text) {
    fputc('"',out);
    for(const unsigned char *p=(const unsigned char *)text;*p;p++) {
        if(*p=='"' || *p=='\\') fprintf(out,"\\%c",*p);
        else if(*p<32) fprintf(out,"\\u%04x",*p);
        else fputc(*p,out);
    }
    fputc('"',out);
}
static void write_error(FILE *out,const counter *event) {
    if(!event->error) { fputs("null",out);return; }
    fputs("{\"stage\":",out);json_string(out,event->stage);
    fprintf(out,",\"errno\":%d,\"message\":",event->error);json_string(out,strerror(event->error));fputc('}',out);
}
static void write_counter(FILE *out,const counter *event) {
    fprintf(out,"{\"type\":%u,\"config\":%" PRIu64 ",\"config_hex\":\"0x%" PRIx64 "\",\"source\":",event->type,event->config,event->config);
    json_string(out,event->source);
    fputs(",\"support_source\":",out);json_string(out,event->support_source);
    fputs(",\"advertised\":",out);
    if(event->advertised<0) fputs("null",out);else fputs(event->advertised?"true":"false",out);
    fputs(",\"raw\":",out);
    if(event->read_ok) fprintf(out,"%" PRIu64,event->raw);else fputs("null",out);
    fputs(",\"scaled\":",out);
    if(valid(event)) fprintf(out,"%.6Lf",scaled(event));else fputs("null",out);
    fputs(",\"time_enabled_ns\":",out);
    if(event->read_ok) fprintf(out,"%" PRIu64,event->enabled);else fputs("null",out);
    fputs(",\"time_running_ns\":",out);
    if(event->read_ok) fprintf(out,"%" PRIu64,event->running);else fputs("null",out);
    fputs(",\"multiplexed\":",out);
    if(valid(event)) fputs(event->running<event->enabled?"true":"false",out);else fputs("null",out);
    fprintf(out,",\"valid\":%s,\"error\":",valid(event)?"true":"false");write_error(out,event);fputc('}',out);
}
static void ratio(FILE *out,unsigned numerator,unsigned denominator,long double multiplier) {
    if(counting && valid(&events[numerator]) && valid(&events[denominator]) && events[denominator].raw)
        fprintf(out,"%.9Lf",multiplier*scaled(&events[numerator])/scaled(&events[denominator]));
    else fputs("null",out);
}
void m8_pmu_write_json(FILE *out,const char *aot_path) {
    fprintf(out,"{\"enabled\":%s,\"scope\":\"calling-thread-user\",\"pinned\":false,\"scaling\":\"raw*time_enabled/time_running\",\"pmu\":",counting?"true":"false");
    if(*arm_name) json_string(out,arm_name);else fputs("null",out);
    fputs(",\"counters\":{",out);
    if(counting) for(unsigned i=0;i<EVENT_COUNT;i++) {
        if(i) fputc(',',out);
        json_string(out,events[i].name);fputc(':',out);write_counter(out,&events[i]);
    }
    fputs("},\"ipc\":",out);ratio(out,INSTRUCTIONS,CYCLES,1);
    fputs(",\"mpki\":{",out);
    const unsigned misses[]={L1I_REFILL,L1D_REFILL,L2D_REFILL,LL_REFILL,ITLB_REFILL,DTLB_REFILL,BRANCH_MISS};
    for(unsigned i=0;i<sizeof misses/sizeof *misses;i++) {
        if(i) fputc(',',out);
        json_string(out,event_names[misses[i]]);fputc(':',out);ratio(out,misses[i],INSTRUCTIONS,1000);
    }
    fputs("},\"stall_shares\":{\"frontend\":",out);ratio(out,FRONTEND_STALL,CYCLES,1);
    fputs(",\"backend\":",out);ratio(out,BACKEND_STALL,CYCLES,1);
    fputs("},\"sampling\":",out);
    if(!samples.requested) fputs("null",out);
    else {
        const char *requested=samples.requested==1?"cycles":samples.requested==2?"l1i-refill":"l2d-refill";
        uint64_t recorded_loss=samples.lost_records;add_saturated(&recorded_loss,samples.lost_samples_records);
        uint64_t lost=samples.lost_read_ok && samples.lost_read>recorded_loss?samples.lost_read:recorded_loss;
        int truncated=samples.truncated || samples.dropped || lost || samples.ring_saturated;
        fputs("{\"requested_event\":",out);json_string(out,requested);
        fputs(",\"event\":",out);json_string(out,samples.event.name);
        fprintf(out,",\"period\":%" PRIu64 ",\"counter\":",samples.period);write_counter(out,&samples.event);
        fprintf(out,",\"sample_type\":\"PERF_SAMPLE_IP\",\"precise_ip\":0,\"ip_precision\":\"overflow IP; skid possible\",\"ring_bytes\":%zu,\"capacity\":%u,\"samples\":%zu,\"lost\":%" PRIu64 ",\"lost_record_count\":%" PRIu64 ",\"lost_samples_record_count\":%" PRIu64 ",\"lost_count_complete\":%s,\"dropped\":%" PRIu64 ",\"ring_saturated\":%s,\"truncated\":%s,\"malformed_records\":%" PRIu64 ",\"other_records\":%" PRIu64 ",\"throttle_records\":%" PRIu64 ",\"unthrottle_records\":%" PRIu64 ",\"profile\":",
            samples.data_size,SAMPLE_CAPACITY,samples.count,lost,samples.lost_records,samples.lost_samples_records,
            samples.lost_read_ok?"true":"false",samples.dropped,samples.ring_saturated?"true":"false",
            truncated?"true":"false",samples.malformed_records,samples.other_records,
            samples.throttle_records,samples.unthrottle_records);
        if(samples.event.error && !samples.count) fputs("null",out);
        else m8_profile_write_pcs_json(out,aot_path,samples.pcs,samples.count);
        fputc('}',out);
    }
    fputc('}',out);
}
void m8_pmu_destroy(void) {
    if(prepared) m8_pmu_stop();
    if(prepared) for(unsigned i=0;i<EVENT_COUNT;i++) if(events[i].fd>=0) close(events[i].fd);
    if(samples.event.fd>=0 && (prepared || samples.requested)) close(samples.event.fd);
    if(samples.mapping) munmap(samples.mapping,samples.map_size);
    free(samples.pcs);memset(&samples,0,sizeof samples);samples.event.fd=-1;
    prepared=counting=started=stopped=0;
}

#ifdef M8_HOST_PMU_TEST
#include <assert.h>
void m8_profile_write_pcs_json(FILE *out,const char *aot,const uintptr_t *pcs,size_t count) {
    (void)aot;(void)pcs;fprintf(out,"{\"test_count\":%zu}",count);
}
static void test_ring_record(sampler *sample,uint64_t position,const void *data,size_t size) {
    size_t offset=(size_t)(position&(sample->data_size-1));
    size_t first=sample->data_size-offset;if(first>size) first=size;
    uint8_t *buffer=(uint8_t *)sample->mapping+sample->data_offset;
    memcpy(buffer+offset,data,first);if(first<size) memcpy(buffer,(const uint8_t *)data+first,size-first);
}
static void tests(void) {
    unsigned event;uint64_t period;
    assert(!parse_sample("cycles:123",&event,&period) && event==1 && period==123);
    assert(!parse_sample("l1i-refill:18446744073709551615",&event,&period) && period==UINT64_MAX);
    const char *bad[]={"","cycles","cycles:","cycles:0","cycles:-1","cycles:+1","cycles:1x","l1d-refill:1","cycles:18446744073709551616","cycles:1:2"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++) { errno=0;assert(parse_sample(bad[i],&event,&period)<0 && errno==EINVAL); }
    counter value={.raw=123,.enabled=1000,.running=250,.read_ok=1,.fd=-1};
    assert(valid(&value) && scaled(&value)==492);
    events[INSTRUCTIONS]=value;counting=1;uint64_t instructions;
    assert(!m8_pmu_instructions(&instructions) && instructions==492);
    events[INSTRUCTIONS].raw=1;events[INSTRUCTIONS].enabled=3;events[INSTRUCTIONS].running=2;
    assert(!m8_pmu_instructions(&instructions) && instructions==2);
    events[INSTRUCTIONS].raw=UINT64_MAX;events[INSTRUCTIONS].enabled=2;events[INSTRUCTIONS].running=1;
    assert(m8_pmu_instructions(&instructions)<0 && errno==ERANGE);
    value.running=0;assert(!valid(&value));value.running=1001;assert(!valid(&value));
    counter short_read={.fd=open("/dev/null",O_RDONLY)};assert(short_read.fd>=0);
    read_counter(&short_read,0,NULL);assert(short_read.error==EIO && !short_read.read_ok && !strcmp(short_read.stage,"read"));close(short_read.fd);
    counter failed_read={.fd=-1};read_counter(&failed_read,0,NULL);assert(failed_read.error==EBADF);
    counter zero_read={.fd=open("/dev/zero",O_RDONLY)};assert(zero_read.fd>=0);
    read_counter(&zero_read,0,NULL);assert(zero_read.error==EAGAIN && zero_read.read_ok && !valid(&zero_read));close(zero_read.fd);
    sampler sample={0};sample.event.fd=-1;sample.data_offset=4096;sample.data_size=128;sample.map_size=4224;
    sample.mapping=calloc(1,sample.map_size);sample.pcs=calloc(SAMPLE_CAPACITY,sizeof *sample.pcs);assert(sample.mapping && sample.pcs);
    struct perf_event_mmap_page *metadata=sample.mapping;
    struct { struct perf_event_header header;uint64_t ip; } ip={{PERF_RECORD_SAMPLE,0,16},0x12345678};
    struct { struct perf_event_header header;uint64_t id,lost; } lost={{PERF_RECORD_LOST,0,24},3,7};
    metadata->data_tail=124;metadata->data_head=164;
    test_ring_record(&sample,124,&ip,sizeof ip);test_ring_record(&sample,140,&lost,sizeof lost);consume_ring(&sample);
    assert(sample.count==1 && sample.pcs[0]==ip.ip && sample.lost_records==7 && metadata->data_tail==164 && !sample.event.error);
    sample.count=SAMPLE_CAPACITY;metadata->data_head=180;test_ring_record(&sample,164,&ip,sizeof ip);consume_ring(&sample);
    assert(sample.dropped==1 && sample.truncated && sample.count==SAMPLE_CAPACITY);
    struct { struct perf_event_header header;uint64_t time,id,stream_id; } throttle={{PERF_RECORD_THROTTLE,0,32},1,2,3};
    sample.truncated=0;metadata->data_head=212;test_ring_record(&sample,180,&throttle,sizeof throttle);consume_ring(&sample);
    assert(sample.throttle_records==1 && sample.truncated);
    throttle.header.type=PERF_RECORD_UNTHROTTLE;metadata->data_head=244;test_ring_record(&sample,212,&throttle,sizeof throttle);consume_ring(&sample);
    assert(sample.unthrottle_records==1);
    struct perf_event_header malformed={PERF_RECORD_SAMPLE,0,4};metadata->data_head=252;test_ring_record(&sample,244,&malformed,sizeof malformed);consume_ring(&sample);
    assert(sample.event.error==EBADMSG && sample.malformed_records==1 && metadata->data_tail==252);
    sample.event.error=0;metadata->data_head=500;consume_ring(&sample);assert(sample.event.error==EOVERFLOW && metadata->data_tail==500);
    free(sample.mapping);free(sample.pcs);
    counting=0;memset(events,0,sizeof events);memset(&samples,0,sizeof samples);samples.event.fd=-1;
}
int main(int argc,char **argv) {
    tests();
    if(argc>1 && !strcmp(argv[1],"--live")) {
        assert(!m8_pmu_prepare(1,"cycles:100000"));assert(!m8_pmu_start());
        volatile uint64_t sink=0;for(unsigned i=0;i<1000000;i++) sink+=i;
        (void)sink;m8_pmu_stop();m8_pmu_write_json(stdout,NULL);fputc('\n',stdout);m8_pmu_destroy();
    } else puts("PMU synthetic scale/read/ring checks PASS");
    return 0;
}
#endif

#else
static int requested_count, requested_sample;
int m8_pmu_prepare(int count,const char *spec) {
    unsigned event;uint64_t period;
    if(count<0 || parse_sample(spec,&event,&period)) { errno=EINVAL;return -1; }
    requested_count=count!=0;requested_sample=event!=0;return 0;
}
int m8_pmu_start(void) { return 0; }
void m8_pmu_stop(void) {}
int m8_pmu_instructions(uint64_t *value) { (void)value;errno=ENOTSUP;return -1; }
void m8_pmu_write_json(FILE *out,const char *aot) {
    (void)aot;fprintf(out,"{\"enabled\":%s,\"sampling_requested\":%s,\"ipc\":null,\"counters\":{},\"sampling\":null,\"error\":{\"errno\":%d,\"message\":\"PMU profiling requires Linux x86-64 or AArch64\"}}",requested_count?"true":"false",requested_sample?"true":"false",ENOTSUP);
}
void m8_pmu_destroy(void) { requested_count=requested_sample=0; }
#endif

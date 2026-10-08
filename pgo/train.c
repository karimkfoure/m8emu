#define _GNU_SOURCE
#include "m8rt.h"
#include "native-live.h"
#include "sha256.h"
#include <dlfcn.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Controller bits follow the M8 Headless 6.5.2 protocol. */
enum { EDIT=1, OPTION=2, RIGHT=4, PLAY=8, SHIFT=16, DOWN=32, UP=64, LEFT=128 };
enum { W=320,H=240,RATE=44100,STEP=100 };
typedef struct {
    char chars[H][W]; uint8_t cyan[H][W], frame[512];
    size_t length; int escaped; unsigned rgb[3]; int firmware_ok;
} screen;
typedef struct {
    m8rt *rt;screen display;double deadline;unsigned held;
    uint8_t *pcm;size_t count,capacity,target;int capture;FILE *raw;
    double started;const char *phase;
    int strict_capture, song_transport;uint64_t next_capture_frame, transport_warm_frames;
    int corpus, corpus_playhead;screen load_display;
} session;
static double now(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9; }
static int le16(const uint8_t *p) { return p[0]|p[1]<<8; }
static int cyan(unsigned r,unsigned g,unsigned b) { return r<=110 && g>=140 && b>=140; }
static void apply(screen *s) {
    const uint8_t *p=s->frame;size_t n=s->length;
    if(p[0]==0xff && n>=6) {
        s->firmware_ok=p[1]==0 && p[2]==6 && p[3]==5 && p[4]==2 && p[5]==0;
    } else if(p[0]==0xfe && (n==5 || n==8 || n==9 || n==12)) {
        int x=le16(p+1),y=le16(p+3),w=1,h=1;
        if(n==9 || n==12) { w=le16(p+5);h=le16(p+7); }
        if(n==8 || n==12) { size_t o=n==8?5:9;for(int i=0;i<3;i++) s->rgb[i]=p[o+i]; }
        int selected=cyan(s->rgb[0],s->rgb[1],s->rgb[2]);
        for(int yy=y;yy<H && yy<y+h;yy++) for(int xx=x;xx<W && xx<x+w;xx++) {
            s->chars[yy][xx]=0;s->cyan[yy][xx]=(uint8_t)selected;
        }
    } else if(p[0]==0xfd && n==12) {
        int x=le16(p+2),y=le16(p+4)+3; /* Cold font 0 has text offset 3. */
        if(x<W && y<H) {
            s->chars[y][x]=p[1]>=32 && p[1]<127?(char)p[1]:'.';
            int selected=cyan(p[6],p[7],p[8]) || cyan(p[9],p[10],p[11]);
            for(int yy=y;yy<H && yy<y+7;yy++) for(int xx=x;xx<W && xx<x+5;xx++) s->cyan[yy][xx]=(uint8_t)selected;
        }
    }
}
static void display_bytes(screen *s,const uint8_t *p,size_t n) {
    for(size_t i=0;i<n;i++) {
        uint8_t b=p[i];
        if(s->escaped) { s->escaped=0;if(b==0xdc) b=0xc0;else if(b==0xdd) b=0xdb;else { s->length=0;continue; } }
        else if(b==0xdb) { s->escaped=1;continue; }
        else if(b==0xc0) { if(s->length) apply(s);s->length=0;continue; }
        if(s->length<sizeof s->frame) s->frame[s->length++]=b;
    }
}
static void row(screen *s,int y,char out[W+1]) {
    size_t n=0;for(int x=0;x<W;x++) if(s->chars[y][x] && s->chars[y][x]!=' ') out[n++]=s->chars[y][x];out[n]=0;
}
static int has(session *s,const char *token) {
    char line[W+1];for(int y=0;y<H;y++) { row(&s->display,y,line);if(strstr(line,token)) return 1; }return 0;
}
static void selected(session *s,char out[W+1]) {
    int best=-1,count=-1;
    for(int y=0;y<H;y++) {
        int chars=0,score=0;for(int x=0;x<W;x++) chars+=s->display.chars[y][x]!=0;
        if(!chars) continue;
        for(int yy=y;yy<H && yy<y+7;yy++) for(int x=0;x<W;x++) score+=s->display.cyan[yy][x];
        if(score>count) { count=score;best=y; }
    }
    if(best<0) out[0]=0;else row(&s->display,best,out);
}
static int playing(session *s) {
    char line[W+1];
    for(int y=0;y<H;y++) {
        row(&s->display,y,line);const char *p=line;if(*p=='<') p++;
        if(*p && strchr("0123456789ABCDEF",*p) && p[1]=='>') return 1;
    }
    return 0;
}
static void dump(session *s) {
    char line[W+1];fprintf(stderr,"Last screen (held=0x%02x pc=0x%08" PRIx64 "):\n",s->held,m8rt_get(s->rt,M8RT_PC));
    for(int y=0;y<H;y++) { row(&s->display,y,line);if(*line) fprintf(stderr,"%s\n",line); }
    selected(s,line);fprintf(stderr,"Selected: %s\n",line);
}
static int fail(session *s,const char *reason) { fprintf(stderr,"FAIL: %s\n",reason);if(s->rt) dump(s);return -1; }
static int require(session *s,const char *token) { if(has(s,token)) return 0;char message[400];snprintf(message,sizeof message,"Expected display token %s",token);return fail(s,message); }
static int drain(session *s) {
    uint8_t bytes[65536];size_t n;
    while((n=m8rt_read_cdc(s->rt,bytes,sizeof bytes))) display_bytes(&s->display,bytes,n);
    m8rt_audio_position position={0};
    while((n=m8rt_read_local_pcm(s->rt,bytes,sizeof bytes,s->strict_capture && s->capture?&position:NULL))) {
        if(s->capture) {
            if(s->strict_capture) {
                if(n!=(size_t)position.frames*4 || position.frame!=s->next_capture_frame) {
                    fprintf(stderr,"PCM discontinuity: next_frame=%" PRIu64 " actual_frame=%" PRIu64 " frames=%u bytes=%zu\n",s->next_capture_frame,position.frame,position.frames,n);
                    return fail(s,"Local PCM block is missing or has an invalid length");
                }
                s->next_capture_frame+=position.frames;
            }
            if(n>s->capacity-s->count) {
                size_t capacity=s->capacity*2;
                if(capacity<s->count+n) capacity=s->count+n;
                void *grown=realloc(s->pcm,capacity);
                if(!grown) return fail(s,"Complete PCM allocation failed");
                s->pcm=grown;s->capacity=capacity;
            }
            memcpy(s->pcm+s->count,bytes,n);s->count+=n;
        }
    }
    while(m8rt_read_pcm(s->rt,bytes,sizeof bytes)) { }
    uint32_t irq[1024];while(m8rt_read_irqs(s->rt,irq,1024)) { }
    if(has(s,"CPUTOOBUSY")) return fail(s,"Firmware CPU TOO BUSY");
    return 0;
}
static int run(session *s,uint64_t iters) {
    double observed=now();
    if(observed>=s->deadline) {
        fprintf(stderr,"Deadline: phase=%s elapsed=%.3f limit=%.3f pcm=%zu target=%zu main_iters=%" PRIu64 " cycles=%" PRIu64 "\n",
            s->phase?s->phase:"setup",observed-s->started,s->deadline-s->started,s->count,s->target,
            m8rt_get(s->rt,M8RT_MAIN_ITERS),m8rt_get(s->rt,M8RT_GUEST_CYCLES));
        return fail(s,"Wall deadline exceeded");
    }
    uint64_t before=m8rt_get(s->rt,M8RT_MAIN_ITERS);
    if(m8rt_run_main_iters(s->rt,iters)<0) return fail(s,m8rt_error(s->rt));
    if(m8rt_get(s->rt,M8RT_MAIN_ITERS)!=before+iters) return fail(s,"Main-loop limit did not complete");
    return drain(s);
}
static int advance(session *s,double seconds) {
    uint64_t cycles=(uint64_t)llround(seconds*600000000.0),start=m8rt_get(s->rt,M8RT_GUEST_CYCLES);
    while(m8rt_get(s->rt,M8RT_GUEST_CYCLES)-start<cycles) {
        uint64_t remaining=cycles-(m8rt_get(s->rt,M8RT_GUEST_CYCLES)-start);
        uint64_t count=remaining/150000;if(!count) count=1;if(count>STEP) count=STEP;
        if(run(s,count)) return -1;
    }return 0;
}
static int mask(session *s,unsigned keys) {
    uint8_t packet[2]={'C',(uint8_t)keys};
    if(m8rt_queue_host_bytes(s->rt,packet,sizeof packet)) return fail(s,"Cannot queue controller packet");
    s->held=keys;
    if(advance(s,.080)) return -1;
    if(m8rt_get(s->rt,M8RT_HOST_QUEUED_BYTES)) return fail(s,"Controller packet remains unconsumed");
    return 0;
}
static int tap(session *s,unsigned key) { return mask(s,key)||mask(s,0)?-1:0; }
static int chord(session *s,unsigned first,unsigned second) {
    return mask(s,first)||mask(s,first|second)||mask(s,first)||mask(s,0)?-1:0;
}
static int transport(session *s) {
    return s->song_transport?chord(s,SHIFT,PLAY):tap(s,PLAY);
}
static int load(session *s,unsigned tracks,int phrase_view) {
    fprintf(stderr,"Boot Headless 6.5.2; wait for SONG at main entry 10000\n");
    while(m8rt_get(s->rt,M8RT_MAIN_ITERS)<10000) if(run(s,STEP)) return -1;
    if(!s->display.firmware_ok) return fail(s,"Expected Headless 6.5.2 with cold font 0");
    fprintf(stderr,"Boot complete; select Project LOAD\n");
    if(require(s,"SONG") || mask(s,0) || chord(s,SHIFT,UP) || require(s,"PROJECT")) return -1;
    char line[W+1];selected(s,line);unsigned direction=strstr(line,"TEMPO")?DOWN:UP;
    unsigned i;
    for(i=0;i<24;i++) { selected(s,line);if(strstr(line,"PROJECTLOADSAVENEW")) break;if(tap(s,direction)) return -1; }
    if(i==24) return fail(s,"Project LOAD row not selected");
    if(tap(s,EDIT)) return -1;
    for(i=0;i<32;i++) {
        selected(s,line);
        if(strstr(line,"DEMO1")) { if(tap(s,EDIT)) return -1;break; }
        if(has(s,"DEMO1.M8S")) { if(tap(s,DOWN)) return -1; }
        else if(!strcmp(line,"/..") || !strcmp(line,"..") || strstr(line,"SONGS") || has(s,"YESNO") || has(s,"OKCANCEL")) { if(tap(s,EDIT)) return -1; }
        else if(tap(s,DOWN)) return -1;
    }
    if(i==32) return fail(s,"Songs/DEMO1.M8S not selected");
    for(i=0;i<10;i++) {
        if(has(s,"SONGLOADED")) break;
        if(has(s,"YESNO") || has(s,"OKCANCEL")) { if(tap(s,EDIT)) return -1; }
        else if(advance(s,.5)) return -1;
    }
    if(require(s,"SONGLOADED")) return -1;
    if(s->corpus) {
        s->load_display=s->display;
        dump(s);
        if(chord(s,SHIFT,DOWN) || require(s,"SONG") || has(s,"LIVE") || s->held) return -1;
        return 0;
    }
    if(require(s,"NAMEDEMO1") || chord(s,SHIFT,DOWN) || require(s,"SONG")) return -1;
    char grid[40]="00";for(unsigned t=0;t<8;t++) { char cell[3];if(t<tracks) snprintf(cell,sizeof cell,"%02X",t);else strcpy(cell,"--");strcat(grid,cell); }
    if(require(s,grid) || chord(s,SHIFT,RIGHT) || require(s,"CHAIN00") || chord(s,SHIFT,RIGHT) || require(s,"PHRASE00")) return -1;
    if(playing(s)) return fail(s,"Loaded phrase is already playing");
    if(!phrase_view && (chord(s,SHIFT,LEFT) || chord(s,SHIFT,LEFT) || require(s,"SONG"))) return -1;
    if(s->held) return fail(s,"Setup left held keys");
    return 0;
}

static void *open_library(const char *path) {
    void *library=dlopen(path,RTLD_NOW|RTLD_LOCAL);
    if(!library) fprintf(stderr,"Library %s: %s\n",path,dlerror());
    return library;
}
int main(int argc,char **argv) {
    int reference=argc==9 && !strcmp(argv[1],"--reference");
    if(reference) {argv++;argc--;}
    if(argc!=8) {
        fprintf(stderr,"Usage: m8-pgo-runner [--reference] FIRMWARE CARD AOT REGIONS HANDLERS SECONDS PCM\n");
        return 2;
    }
    char *end;double seconds=strtod(argv[6],&end);
    if(*end || !isfinite(seconds) || seconds<1 || seconds>60) return 2;
    session *s=calloc(1,sizeof *s);if(!s) return 1;
    s->started=now();s->deadline=s->started+600;s->phase="boot/load";
    s->corpus=1;s->song_transport=1;s->strict_capture=1;
    s->target=(size_t)llround(seconds*RATE)*4;
    int rc=1;void *aot=NULL,*regions=NULL,*handlers=NULL;
    const struct { const char *name,*value; } environment[]={
        {"M8RT_STALL_POLICY","deterministic"},{"M8RT_AOT_TIMING","0"},
        {"M8RT_NATIVE_EXCLUSIVE_TRANSFER","1"},{"M8RT_NATIVE_DEVICE_DIRECT","1"},
        {"M8RT_NATIVE_EVENT_LOOP","1"},{"M8RT_NATIVE_SERVICE_CONTINUE","1"},
        {"M8RT_NATIVE_DEADLINE_YIELD","1"}};
    for(unsigned i=0;i<sizeof environment/sizeof environment[0];i++)
        if(setenv(environment[i].name,environment[i].value,1)) goto done;
    m8rt_native_live_config config={0};
    m8_aot_fn (*lookup)(uint32_t)=NULL;
    uint32_t (*entries)(const uint32_t **)=NULL;
    uint32_t (*services)(void)=NULL;
    if(!reference) {
    aot=open_library(argv[3]);regions=open_library(argv[4]);handlers=open_library(argv[5]);
    if(!aot || !regions || !handlers) goto done;
    lookup=dlsym(aot,"m8_aot_lookup");
    entries=dlsym(aot,"m8_aot_entries");
    services=dlsym(aot,"m8_aot_memory_services");
    uint32_t (*fixed)(void)=dlsym(aot,"m8_aot_fixed_memory_required");
    void (*fallback)(int)=dlsym(aot,"m8_aot_set_slow_fallback");
    config.step=dlsym(handlers,"m8_native_step");
    config.verify=dlsym(aot,"m8_aot_verify_memory");
    config.region=dlsym(regions,"m8_native_region_step");
    config.region_verify=dlsym(regions,"m8_native_region_verify_memory");
    config.chain=dlsym(regions,"m8_native_region_chain");
    size_t (*slots)(void)=dlsym(regions,"m8_native_region_chain_cache_slots");
    config.event_chain=dlsym(regions,"m8_native_region_event_chain");
    if(!lookup || !entries || !fixed || !fixed() || !config.step || !config.verify ||
       !config.region || !config.region_verify || !config.chain || !slots || !config.event_chain) {
        fprintf(stderr,"Generated libraries lack the native training contract\n");goto done;
    }
    config.chain_cache_slots=slots();if(fallback) fallback(0);
    }
    m8rt_options options={0,1,1,0};
    s->rt=m8rt_create_aot(argv[1],argv[2],&options,M8RT_AOT_REQUIRE_FIXED_MEMORY);
    if(!s->rt || *m8rt_error(s->rt)) {fprintf(stderr,"%s\n",m8rt_error(s->rt));goto done;}
    if(m8rt_nor_set_file(s->rt,NULL) || m8rt_nor_set_output(s->rt,NULL) ||
       m8rt_set_audio_tap(s->rt,1) || m8rt_set(s->rt,1,1) || load(s,0,0)) goto done;
    if(!reference && (m8rt_set_aot(s->rt,lookup,entries) ||
       m8rt_set_aot_services(s->rt,services?services():0) ||
       m8rt_set_native_live(s->rt,&config))) {fail(s,"Cannot activate native training");goto done;}
    s->phase="playback";s->capture=1;
    s->next_capture_frame=m8rt_get(s->rt,M8RT_LOCAL_PCM_BYTES)/4;
    if(transport(s) || advance(s,seconds)) goto done;
    uint64_t iters=m8rt_get(s->rt,M8RT_MAIN_ITERS),cycles=m8rt_get(s->rt,M8RT_GUEST_CYCLES),pc=m8rt_get(s->rt,M8RT_PC);
    m8rt_native_live_stats stats={0};
    if(!reference && (m8rt_read_native_live_stats(s->rt,&stats) || !stats.enters || stats.rejected_entries || stats.code_disabled)) {
        fail(s,"Native training rejected the execution contract");goto done;
    }
    s->capture=0;
    if(transport(s) || advance(s,.5) || playing(s) || s->held) {fail(s,"STOP left transport or keys active");goto done;}
    FILE *output=fopen(argv[7],"wb");if(!output) {perror(argv[7]);goto done;}
    int error=fwrite(s->pcm,1,s->count,output)!=s->count;
    if(fclose(output)) error=1;if(error) {perror(argv[7]);goto done;}
    sha256 hash;char hex[65];sha_init(&hash);sha_update(&hash,s->pcm,s->count);sha_finish(&hash,hex);
    unsigned nonzero=0;for(size_t i=0;i<s->count;i++) nonzero|=s->pcm[i];
    printf("{\"pcm_bytes\":%zu,\"pcm_sha256\":\"%s\",\"end_iters\":%" PRIu64 ",\"end_cycles\":%" PRIu64 ",\"end_pc\":%" PRIu64 ",\"non_silent\":%s,\"native_enters\":%" PRIu64 ",\"native_leaves\":%" PRIu64 ",\"checks_passed\":true}\n",
        s->count,hex,iters,cycles,pc,nonzero?"true":"false",stats.enters,stats.leaves);
    rc=0;
done:
    if(s->rt) m8rt_destroy(s->rt);
    if(handlers) dlclose(handlers);if(regions) dlclose(regions);if(aot) dlclose(aot);
    free(s->pcm);free(s);return rc;
}

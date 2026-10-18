#include "checkpoint.h"
#include "session-codec.h"
#include "storage.h"
#include <SDL3/SDL.h>
#include <stdio.h>

extern void m8_demo_session(m8_demo *,m8_session_io *);
typedef struct {
    m8_card *card;
    m8rt_session_raw *raw,*retired;
    m8_session host;
    m8_checkpoint checkpoint;
    uint8_t *pcm;
    size_t pcm_size;
    SDL_Thread *thread;
    _Atomic int done;
    int result;
    uint64_t generation,begin,end;
    char error[512];
} store_job;
static void host_state(m8_session *,m8_session_io *,uint8_t **,size_t *);
static int store(void *context) {
    store_job *job=context;
    m8rt_session_raw_release(job->retired);job->retired=NULL;
    m8rt_session_blob packet=m8rt_session_raw_packet(job->raw),sd=m8rt_session_raw_media(job->raw,7),nor=m8rt_session_raw_media(job->raw,9),blob={0};
    size_t capacity=2u*1024*1024+job->pcm_size;uint8_t *payload=malloc(capacity);
    m8rt_session_error error={0};int result=M8RT_SESSION_NOMEM;
    if(payload&&sd.data&&nor.data) {
        m8_session_io c={.data=payload,.size=capacity};host_state(&job->host,&c,&job->pcm,&job->pcm_size);
        m8rt_session_section section={M8RT_SESSION_SDL,1,payload,c.at};
        result=c.result?c.result:m8rt_session_raw_encode(packet.data,packet.size,sd.data,sd.size,nor.data,nor.size,&section,1,&blob,&error);
    }
    if(result)snprintf(job->error,sizeof job->error,"Session encode refused (%d): %s",result,error.predicate?error.predicate:"host state");
    else if((result=m8_card_session_save(job->card,blob.data,blob.size)))snprintf(job->error,sizeof job->error,"%s",m8_card_error(job->card));
    m8rt_session_release(&blob);m8rt_session_raw_release(job->raw);
    m8_demo_destroy(job->host.demo);free(job->pcm);free(payload);
    job->result=result;job->end=SDL_GetTicksNS();atomic_store_explicit(&job->done,1,memory_order_release);return result;
}
static void finish_store(m8_checkpoint *p,int wait) {
    store_job *job=p->store_job;
    if(!job||(!wait&&!atomic_load_explicit(&job->done,memory_order_acquire)))return;
    SDL_WaitThread(job->thread,NULL);
    if(job->result)snprintf(p->error,sizeof p->error,"%s",job->error);
    else {p->error[0]=0;p->committed_ns=job->end;p->committed_changes=job->generation;}
    if(job->end-job->begin>p->total_ns)p->total_ns=job->end-job->begin;
    free(job);p->store_job=NULL;
}
static void host_state(m8_session *s,m8_session_io *c,uint8_t **pcm,size_t *pcm_size) {
    m8_checkpoint *p=s->checkpoint;M8WebDisplay *d=&p->screen;
    uint32_t version=(uint32_t)session_number(c,1,4);
    if(version!=1){c->result=M8RT_SESSION_INCOMPATIBLE;return;}
    S32(d->width);S32(d->height);S32(d->font);S32(d->packet_size);S32(d->waveform_size);
    S32(d->rectangle_color);S32(d->background_color);S64(d->dirty_serial);S64(d->packets);S64(d->errors);
    S32(d->escaped);S32(d->discard);S32(d->waveform_cleared);
    if(c->pass==2 && (!d->width||d->width>480||!d->height||d->height>320||d->font>4||
        d->packet_size>sizeof d->packet||d->waveform_size>480||d->escaped>1||d->discard)) {c->result=M8RT_SESSION_INVALID;return;}
    session_bytes(c,d->pixels,(size_t)d->width*d->height*4);
    session_bytes(c,d->packet,sizeof d->packet);
    m8_demo_session(s->demo,c);
    uint64_t n=session_number(c,*pcm_size,8);
    if(n>88200u*4u||n%4||(c->data&&n>c->size-c->at)){c->result=M8RT_SESSION_INVALID;return;}
    if(c->pass==2) {
        if(s->capacity<n/4+64)s->capacity=(uint32_t)(n/4+64);
        *pcm=n?malloc((size_t)n):NULL;*pcm_size=(size_t)n;
        if(n&&!*pcm){c->result=M8RT_SESSION_NOMEM;return;}
    }
    session_bytes(c,*pcm,(size_t)n);
}
int m8_checkpoint_open(m8_session *s,m8_card *card,m8rt *(*fresh)(void *),void *context,int restore) {
    m8_checkpoint *p=calloc(1,sizeof *p);if(!p)return -1;
    s->checkpoint=p;p->card=card;m8_display_init(&p->screen);p->last_input_ns=SDL_GetTicksNS();p->changes=1;
    if(!restore||!card||m8_card_read_only(card))return 0;
    void *bytes=NULL;size_t n=0;int loaded=m8_card_session_load(card,&bytes,&n);
    if(loaded) {if(loaded<0)snprintf(p->error,sizeof p->error,"%s",m8_card_error(card));return 0;}
    m8rt_session_error error={0};m8rt_session_section section,sd,nor;
    m8rt_session_info info;
    if(!m8rt_session_describe(bytes,n,&info,&error)&&info.sections==2&&
       !m8rt_session_find(bytes,n,M8RT_SESSION_SD,&sd,&error)&&
       !m8rt_session_find(bytes,n,M8RT_SESSION_NOR,&nor,&error)&&nor.size==63u*4096u) {
        /* A committed card replacement retains media for a deliberate fresh
         * boot. It has no old machine state to restore or report as lost. */
        int failed=m8rt_nor_set_output(s->rt,NULL)||m8rt_sd_replace_image(s->rt,sd.data,sd.size)||
            m8rt_mem_write(s->rt,0x607c0000u,nor.data,nor.size);
        free(bytes);return failed?-1:0;
    }
    int result=m8rt_session_find(bytes,n,M8RT_SESSION_SDL,&section,&error);
    s->demo=m8_demo_create(s->rt,m8_session_pump,s);
    if(!s->demo){free(bytes);return -1;}
    if(!result) {
        m8_session_io c={.data=(uint8_t *)section.data,.size=section.size,.pass=2};
        host_state(s,&c,&s->initial_pcm,&s->initial_pcm_size);
        result=c.result?c.result:c.at!=c.size?M8RT_SESSION_INVALID:0;
    }
    if(!result)result=m8rt_session_restore(s->rt,bytes,n,&error);
    if(!result) {
        p->restored=p->paused=1;s->resume_paused=1;p->committed_ns=SDL_GetTicksNS();p->committed_changes=p->changes;
        free(bytes);return 1;
    }
    /* Restore only touched a disposable instance. Normal boot starts in a new
     * runtime with the captured generation's media when those sections validate. */
    int have_media=!m8rt_session_find(bytes,n,7,&sd,&error)&&!m8rt_session_find(bytes,n,9,&nor,&error)&&nor.size==63u*4096u;
    m8rt_destroy(s->rt);s->rt=fresh(context);
    m8_demo_destroy(s->demo);s->demo=NULL;free(s->initial_pcm);s->initial_pcm=NULL;s->initial_pcm_size=0;
    m8_display_init(&p->screen);
    if(!s->rt){free(bytes);return -1;}
    if(have_media && (m8rt_sd_replace_image(s->rt,sd.data,sd.size)||
       m8rt_mem_write(s->rt,0x607c0000u,nor.data,nor.size))) {free(bytes);return -1;}
    snprintf(p->error,sizeof p->error,"Session restore refused (%d); normal boot%s. Unsaved M8 edits were lost.",result,have_media?" uses retained SD and settings":" uses the card");
    free(bytes);return 0;
}
int m8_checkpoint_feed(m8_session *s,const void *bytes,size_t n) {
    return s->checkpoint?m8_display_feed(&s->checkpoint->screen,bytes,n):0;
}
int m8_checkpoint_seed(m8_session *s) {
    m8_checkpoint *p=s->checkpoint;if(!p||!p->card||m8_card_read_only(p->card))return 0;
    m8rt_session_error error={0};m8rt_session_raw *seed=NULL;
    int result=m8rt_session_raw_seed(s->rt,&seed,&error);
    if(result){snprintf(p->error,sizeof p->error,"Media seed refused (%d): %s",result,error.predicate?error.predicate:"media");return -1;}
    m8rt_session_raw_release(p->raw);p->raw=seed;
    p->sd_revision=m8rt_sd_image_revision(s->rt);p->nor_revision=m8rt_nor_image_revision(s->rt);return 0;
}
void m8_checkpoint_input(m8_session *s,uint64_t now) {
    m8_checkpoint *p=s->checkpoint;if(!p)return;
    p->last_input_ns=now;++p->changes;
}
void m8_checkpoint_pcm(m8_session *s,const void *bytes,size_t n) {
    m8_checkpoint *p=s->checkpoint;if(!p)return;
    const uint8_t *data=bytes;
    for(size_t i=0;i+3<n;i+=4) {
        if(data[i]|data[i+1]|data[i+2]|data[i+3]) {p->silent_frames=0;++p->changes;}
        else if(p->silent_frames<88200)p->silent_frames++;
    }
}
int m8_checkpoint_idle(const m8_checkpoint *p,uint64_t now) {
    return p && p->silent_frames>=88200 && now>=p->last_input_ns && now-p->last_input_ns>=UINT64_C(5000000000);
}
m8rt_session_result m8_checkpoint_save(m8_session *s,int force) {
    m8_checkpoint *p=s->checkpoint;if(!p||!p->card||m8_card_read_only(p->card))return M8RT_SESSION_OK;
    finish_store(p,force==2);if(p->store_job)return M8RT_SESSION_OK;
    uint64_t now=SDL_GetTicksNS(),sd=m8rt_sd_image_revision(s->rt),nor=m8rt_nor_image_revision(s->rt);
    if(sd!=p->sd_revision||nor!=p->nor_revision){++p->changes;p->sd_revision=sd;p->nor_revision=nor;}
    if(force!=2 && (!m8_checkpoint_idle(p,now)||p->changes==p->committed_changes))return M8RT_SESSION_OK;
    if(force!=2&&!p->raw)return M8RT_SESSION_OK; /* Failed media seeding must not cause a large live copy. */
    /* Bound repeated storage failures without changing the idle predicate. */
    if(force!=2 && p->last_attempt && now-p->last_attempt<UINT64_C(5000000000))return M8RT_SESSION_OK;
    p->last_attempt=now;
    store_job *job=calloc(1,sizeof *job);if(!job){snprintf(p->error,sizeof p->error,"Cannot allocate checkpoint");return M8RT_SESSION_NOMEM;}
    job->begin=now;job->card=p->card;job->generation=p->changes;
    job->host.capacity=s->capacity;job->checkpoint.screen=p->screen;
    job->host.checkpoint=&job->checkpoint;job->host.demo=m8_demo_copy(s->demo);
    size_t capacity=(size_t)s->capacity*4;job->pcm=capacity?malloc(capacity):NULL;
    job->pcm_size=s->audio?m8live_copy_pending(s->audio,job->pcm,capacity):s->initial_pcm_size;
    if(!s->audio&&job->pcm_size&&job->pcm)memcpy(job->pcm,s->initial_pcm,job->pcm_size);
    m8rt_session_error error={0};
    int result=(!job->host.demo||(capacity&&!job->pcm)||job->pcm_size==SIZE_MAX)?M8RT_SESSION_NOMEM:
        m8rt_session_raw_capture(s->rt,p->raw,&job->raw,&error);
    if(result) {
        snprintf(p->error,sizeof p->error,"Session capture refused (%d): %s",result,error.predicate?error.predicate:"host state");
        m8_demo_destroy(job->host.demo);free(job->pcm);free(job);return (m8rt_session_result)result;
    }
    job->retired=p->raw;p->raw=job->raw;m8rt_session_raw_retain(p->raw);
    atomic_init(&job->done,0);job->thread=SDL_CreateThread(store,"m8-session-store",job);
    if(!job->thread){m8rt_session_raw_release(job->retired);m8rt_session_raw_release(job->raw);m8_demo_destroy(job->host.demo);free(job->pcm);free(job);snprintf(p->error,sizeof p->error,"Cannot start session storage");return M8RT_SESSION_NOMEM;}
    p->store_job=job;++p->captures;if(force!=2)++p->idle_captures;
    uint64_t elapsed=SDL_GetTicksNS()-now;if(elapsed>p->capture_ns)p->capture_ns=elapsed;
    return M8RT_SESSION_OK;
}
void m8_checkpoint_wait(m8_session *s) {if(s->checkpoint)finish_store(s->checkpoint,1);}
void m8_checkpoint_close(m8_session *s) {
    if(!s->checkpoint)return;
    finish_store(s->checkpoint,1);m8rt_session_raw_release(s->checkpoint->raw);free(s->checkpoint);s->checkpoint=NULL;
    free(s->initial_pcm);s->initial_pcm=NULL;s->initial_pcm_size=0;
}

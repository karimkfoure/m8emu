#include "session.h"
#include "checkpoint.h"
#include "card.h"
#include "fat.h"
#include "storage.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int discard_display(void *context,const void *bytes,size_t size) {
    (void)context;(void)bytes;(void)size;return 0;
}

m8_session_result m8_session_replace(m8_session *s,m8_card *card,m8_session_factory fresh,void *context,
    const void *bytes,size_t size,int reset,char *error,size_t capacity) {
    if(!s->explorer_paused||!fresh)return M8_SESSION_INVALID;
    if(card&&m8_card_read_only(card)){snprintf(error,capacity,"The SD card is read-only.");return M8_SESSION_STORAGE;}
    if(s->observe){snprintf(error,capacity,"Stop PCM or latency recording before replacing the SD card.");return M8_SESSION_UNSUPPORTED;}
    m8_fat_card *fat=NULL;m8_fat_error fat_error;
    if(m8_fat_open(bytes,size,&fat,&fat_error)!=M8_FAT_OK) {
        snprintf(error,capacity,"%s",fat_error.message);return M8_SESSION_INVALID;
    }
    m8_fat_close(fat);
    /* Prepare to an absolute PCM frame boundary while output is paused.
     * A wall deadline is only needed by the subsequent live service loop. */
    m8_session candidate={.capacity=s->capacity,.high=s->high,.low=s->low,.slice_us=s->slice_us,.display=discard_display};
    m8_session_result result=M8_SESSION_NOMEM;
    unsigned char *nor=malloc(63u*4096u),*pcm=malloc((size_t)s->capacity*4);
    m8rt_session_blob media={0};m8rt_session_error cause={0};size_t pending=0;
    if(!nor||!pcm){snprintf(error,capacity,"Cannot allocate the new session.");goto done;}
    if(reset)memset(nor,255,63u*4096u);
    else if(m8rt_mem_read(s->rt,0x607c0000u,nor,63u*4096u)) {
        snprintf(error,capacity,"Cannot copy M8 settings.");result=M8_SESSION_GUEST_FAILED;goto done;
    }
    candidate.rt=fresh(context,error,capacity);
    if(!candidate.rt){result=M8_SESSION_GUEST_FAILED;goto done;}
    if(m8rt_nor_set_output(candidate.rt,NULL)||m8rt_sd_replace_image(candidate.rt,bytes,size)||
       m8rt_mem_write(candidate.rt,0x607c0000u,nor,63u*4096u)) {
        snprintf(error,capacity,"Cannot prepare replacement media: %s",m8rt_error(candidate.rt));result=M8_SESSION_GUEST_FAILED;goto done;
    }
    result=(m8_session_result)m8rt_media_capture(candidate.rt,&media,&cause);
    if(result){snprintf(error,capacity,"Replacement media refused (%d): %s",result,cause.predicate?cause.predicate:"media");goto done;}
    if(m8_checkpoint_open(&candidate,card,NULL,NULL,0)<0){result=M8_SESSION_NOMEM;goto done;}
    candidate.demo=m8_demo_create(candidate.rt,m8_session_pump,&candidate);
    if(!candidate.demo||m8_demo_enable_selection(candidate.demo)<0){result=M8_SESSION_NOMEM;goto done;}
    if(m8_demo_boot_paused(candidate.demo)<0) {
        snprintf(error,capacity,"Replacement boot failed: %s",m8_demo_error(candidate.demo));result=M8_SESSION_GUEST_FAILED;goto done;
    }
    candidate.audio=m8live_create(candidate.capacity,0);
    if(!candidate.audio){result=M8_SESSION_NOMEM;goto done;}
    candidate.delivering=1;candidate.progress=SDL_GetTicksNS();
    candidate.output_origin=candidate.last_frames=m8rt_get(candidate.rt,M8RT_LOCAL_PCM_BYTES)/4;
    if(m8_session_fill(&candidate,M8_SESSION_PREPARE)<0||m8_checkpoint_seed(&candidate)<0) {
        snprintf(error,capacity,"Replacement preparation failed: %s",*candidate.error?candidate.error:
            candidate.checkpoint?candidate.checkpoint->error:"audio");result=M8_SESSION_GUEST_FAILED;goto done;
    }
    pending=m8live_copy_pending(candidate.audio,pcm,(size_t)s->capacity*4);
    if(pending==SIZE_MAX){snprintf(error,capacity,"Replacement PCM exceeds output capacity.");result=M8_SESSION_INVALID;goto done;}
    if(card&&m8_card_replace(card,bytes,size,media.data,media.size)) {
        snprintf(error,capacity,"%s",m8_card_error(card));result=M8_SESSION_STORAGE;goto done;
    }
    /* No guest execution or fallible allocation remains after card commit.
     * Keep the output object stable for UI readers. Its old PCM belongs to
     * the discarded runtime, and must not play after the fresh boot. */
    m8_checkpoint_close(s);m8_demo_destroy(s->demo);m8rt_destroy(s->rt);
    s->rt=candidate.rt;candidate.rt=NULL;s->demo=candidate.demo;candidate.demo=NULL;
    m8_demo_bind(s->demo,s->rt,m8_session_pump,s);
    s->checkpoint=candidate.checkpoint;candidate.checkpoint=NULL;
    s->last_frames=candidate.last_frames;s->output_origin=candidate.output_origin;
    s->produced_base=candidate.last_frames;s->service_seconds=0;s->service_calls=0;s->playhead_rows=0;s->resume_paused=0;
    error[0]=0;
    if(card&&*m8_card_error(card))snprintf(error,capacity,"%s",m8_card_error(card));
    if(m8live_discard_pending(s->audio)<0||m8live_write(s->audio,pcm,pending/4)<0) {
        snprintf(error,capacity,"The new card is committed; audio could not be prepared.");
        s->explorer_paused=0;s->resume_paused=1;
        if(s->checkpoint)s->checkpoint->paused=1;
    } else {
        s->explorer_audio_running=1;
    }
    if(s->checkpoint&&*error)snprintf(s->checkpoint->error,sizeof s->checkpoint->error,"%s",error);
    result=M8_SESSION_OK;
done:
    if(result==M8_SESSION_NOMEM&&!*error)snprintf(error,capacity,"Cannot allocate the replacement session.");
    m8_checkpoint_close(&candidate);m8_demo_destroy(candidate.demo);m8live_destroy(candidate.audio);m8rt_destroy(candidate.rt);
    m8rt_session_release(&media);free(nor);free(pcm);return result;
}

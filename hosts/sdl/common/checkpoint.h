#ifndef M8_HOST_CHECKPOINT_H
#define M8_HOST_CHECKPOINT_H
#include "session.h"
#include "card.h"
#include "../../common/display.h"
#include <stdatomic.h>
typedef struct m8_checkpoint {
    m8_card *card;
    M8WebDisplay screen;
    uint64_t last_attempt,committed_ns,capture_ns,total_ns,last_input_ns;
    uint64_t silent_frames,changes,committed_changes,sd_revision,nor_revision;
    uint64_t captures,idle_captures;
    m8rt_session_raw *raw;
    int restored,paused;
    void *store_job;
    char error[512];
} m8_checkpoint;
/* Call on the main thread before starting the execution owner. fresh creates
 * a configured runtime without executing it, used only after restore refusal. */
int m8_checkpoint_open(m8_session *,m8_card *,m8rt *(*fresh)(void *),void *,int restore);
int m8_checkpoint_feed(m8_session *,const void *,size_t);
/* force 0: automatic; 1: idle-only hide/minimize; 2: explicit/clean close. */
m8rt_session_result m8_checkpoint_save(m8_session *,int force);
int m8_checkpoint_seed(m8_session *);
void m8_checkpoint_input(m8_session *,uint64_t now);
void m8_checkpoint_pcm(m8_session *,const void *,size_t bytes);
int m8_checkpoint_idle(const m8_checkpoint *,uint64_t now);
void m8_checkpoint_wait(m8_session *);
void m8_checkpoint_close(m8_session *);
int m8_bridge_restore_display(const M8WebDisplay *,int render);
#endif

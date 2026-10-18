#ifndef M8_HOST_SESSION_SCRIPT_H
#define M8_HOST_SESSION_SCRIPT_H
#include "session.h"
#include <stdatomic.h>
typedef struct m8_session_script_stats {
    uint64_t pcm_bytes, display_bytes, blocks, first_frame, end_frame;
    unsigned commands, complete;
} m8_session_script_stats;
/* Guest-owner diagnostic; no audio consumer and no wall-time pacing.
 * Output files are created exclusively in an existing directory. */
int m8_session_script_run(m8_session *s, const char *script, const char *output,
                          const _Atomic int *stop, m8_session_script_stats *stats);
#endif

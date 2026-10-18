#ifndef M8_NATIVE_DEMO_H
#define M8_NATIVE_DEMO_H

#include "m8rt.h"

typedef struct m8_demo m8_demo;
/* Service CDC, PCM and host events after each bounded runtime slice. Feed the
 * CDC bytes to m8_demo_feed_cdc as well as the renderer. Return -1 on failure. */
typedef int (*m8_demo_pump_fn)(void *context);

enum m8_demo_keys {
    M8_DEMO_EDIT = 0x01, M8_DEMO_OPTION = 0x02, M8_DEMO_RIGHT = 0x04,
    M8_DEMO_PLAY = 0x08, M8_DEMO_SHIFT = 0x10, M8_DEMO_DOWN = 0x20,
    M8_DEMO_UP = 0x40, M8_DEMO_LEFT = 0x80
};

/* rt must use the app's local audio tap. The pump drains its completed blocks
 * during setup, with capture enabled before guest execution. */
m8_demo *m8_demo_create(m8rt *rt, m8_demo_pump_fn pump, void *context);
void m8_demo_destroy(m8_demo *demo);
/* Immutable observer copy for session encoding on the storage thread. */
m8_demo *m8_demo_copy(const m8_demo *);
/* Rebind a prepared observer when its session moves to the active owner. */
void m8_demo_bind(m8_demo *,m8rt *,m8_demo_pump_fn,void *);
const char *m8_demo_error(const m8_demo *demo);
int m8_demo_feed_cdc(m8_demo *demo, const void *bytes, size_t count);
/* Boot accepts only the supported Headless 6.5.2 display identity. It releases
 * keys, checks Song view, and does not change a supplied card's song. */
int m8_demo_boot(m8_demo *demo);
/* Paused replacement preparation: fixed guest frames, no wall-time slices. */
int m8_demo_boot_paused(m8_demo *demo);
/* For a fresh bundled card only: build C-5/E-5/G-5/C-6 on rows 0/4/8/C of
 * Phrase 01 with Wavsynth 01, volume 64, and OFF at row F. Leave it stopped. */
int m8_demo_prepare_simple(m8_demo *demo);
/* Load a short project name from /Songs through the firmware file browser.
 * Start at stopped Song; finish at the first track's stopped Phrase. */
int m8_demo_load_project(m8_demo *demo, const char *name);
/* The current Phrase playhead determines whether PLAY is needed. */
int m8_demo_transport(m8_demo *demo, int playing);
/* Ordered controls with an 80 ms guest-clock dwell for each transition. */
int m8_demo_tap(m8_demo *demo, unsigned mask);
int m8_demo_advance(m8_demo *demo, double seconds);
/* Compact text lines omit spaces. Return 1 for a line, 0 at the end. */
int m8_demo_line(m8_demo *demo, unsigned line, char *out, size_t capacity);
int m8_demo_playhead(m8_demo *demo);
/* Return the most recently drawn cyan selection rectangle's position. */
int m8_demo_cursor(const m8_demo *demo, unsigned *x, unsigned *y);
unsigned m8_demo_held_keys(const m8_demo *demo);
/* Diagnostic display state. Selection storage is optional in ordinary use. */
int m8_demo_enable_selection(m8_demo *demo);
int m8_demo_selected(m8_demo *demo, char *out, size_t capacity);
int m8_demo_identity(const m8_demo *demo, unsigned *model,
                     unsigned *major, unsigned *minor, unsigned *patch, unsigned *font);

#endif

#ifndef M8_HOST_BRIDGE_H
#define M8_HOST_BRIDGE_H
#include "m8rt.h"
#include "audio.h"
/* m8c has one renderer/input session. All calls stay on the SDL main thread. */
void m8_bridge_attach(m8rt *runtime, int render);
void m8_bridge_transport(int (*send)(void *, const void *, size_t), void *context);
void m8_bridge_reset_decoder(void);
int m8_bridge_restore_parser(const void *,size_t,unsigned);
void m8_bridge_audio(m8live_audio *audio);
int m8_bridge_feed(const void *bytes, size_t n);
uint64_t m8_bridge_packets(void);
unsigned m8_bridge_held(void);
unsigned m8_bridge_host_held(void);
/* Pointer keys have a separate owner. Releases preserve keyboard/gamepad holds. */
int m8_bridge_keypad(unsigned mask);
#endif

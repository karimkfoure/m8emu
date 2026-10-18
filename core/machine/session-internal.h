#ifndef M8RT_SESSION_INTERNAL_H
#define M8RT_SESSION_INTERNAL_H
#include "internal.h"
#include "session-codec.h"
void m8rt_session_queue(m8_session_io *,m8rt_queue *,unsigned unit);
void m8rt_session_words(m8_session_io *,uint32_t *,size_t);
void m8rt_session_mmio(m8rt *,m8_session_io *);
void m8rt_session_usb(m8rt *,m8_session_io *);
void m8rt_session_sd(m8rt *,m8_session_io *);
void m8rt_session_nor(m8rt *,m8_session_io *);
m8rt_session_result m8rt_session_sd_media(m8rt *,m8rt_session_error *);
bool m8rt_session_nor_busy(m8rt *);
int m8rt_session_sd_view(m8rt *,const void *,size_t);
bool m8rt_session_sd_page_changed(m8rt *,size_t,uint64_t);
#endif

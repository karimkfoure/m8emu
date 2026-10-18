#ifndef M8RT_SESSION_CODEC_H
#define M8RT_SESSION_CODEC_H
#include <m8emu/m8rt.h>
#include <stdlib.h>
#include <string.h>
/* pass: 0 encode/count, 1 validate without mutation, 2 install in candidate. */
typedef struct {
    uint8_t *data;
    size_t size, at;
    unsigned pass;
    unsigned raw; /* Transient copy: native byte order, dense register banks. */
    m8rt_session_result result;
} m8_session_io;
static inline void session_bytes(m8_session_io *c,void *p,size_t n) {
    if(c->result) return;
    if(n>SIZE_MAX-c->at || (c->data && (c->at>c->size || n>c->size-c->at))) {
        c->result=M8RT_SESSION_INVALID;return;
    }
    if(c->data && n) {
        if(!c->pass) memcpy(c->data+c->at,p,n);
        else if(c->pass==2) memcpy(p,c->data+c->at,n);
    }
    c->at+=n;
}
static inline uint64_t session_number(m8_session_io *c,uint64_t v,unsigned n) {
    if(c->raw) {
        uint64_t out=v;session_bytes(c,&out,n);return out;
    }
    uint8_t b[8]={0};
    for(unsigned i=0;i<n;i++) b[i]=(uint8_t)(v>>(8*i));
    if(c->pass && c->data && !c->result && c->at<=c->size && n<=c->size-c->at)
        memcpy(b,c->data+c->at,n);
    session_bytes(c,b,n);
    uint64_t out=0;for(unsigned i=0;i<n;i++) out|=(uint64_t)b[i]<<(8*i);
    return out;
}
#define SNUM(field,n) do {uint64_t session_value=session_number(c,(uint64_t)(field),n);if(c->pass==2) (field)=session_value;} while(0)
#define S8(field) SNUM(field,1)
#define S16(field) SNUM(field,2)
#define S32(field) SNUM(field,4)
#define S64(field) SNUM(field,8)
#define SB(field) do {uint64_t session_value=session_number(c,(field),1);if(session_value>1)c->result=M8RT_SESSION_INVALID;if(c->pass==2)(field)=session_value!=0;} while(0)
#define SI32(field) do {uint32_t v=(uint32_t)session_number(c,(uint32_t)(field),4);if(c->pass==2)(field)=v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;} while(0)
#define SSZ(field) do {uint64_t v=session_number(c,(field),8);if(v>SIZE_MAX)c->result=M8RT_SESSION_INCOMPATIBLE;if(c->pass==2)(field)=(size_t)v;} while(0)
#endif

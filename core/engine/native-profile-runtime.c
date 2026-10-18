#include "internal.h"
#include <stdlib.h>
#include <string.h>
#ifdef M8RT_INTERPRETER
#include "native-interpreter-profile.h"
#endif
#ifndef M8RT_INTERPRETER_PROFILE_DEFAULT
#define M8RT_INTERPRETER_PROFILE_DEFAULT 0
#endif

int m8rt_set_interpreter_profile(m8rt *rt,int enabled) {
#ifdef M8RT_INTERPRETER
    if(!rt || rt->in_emu) return -1;
    if(enabled && !rt->interpreter_profile) {
        rt->interpreter_profile=m8_native_interpreter_profile_create(0);
        if(!rt->interpreter_profile) return -1;
    }
    m8_native_interpreter_profile_enable(rt->interpreter_profile,enabled!=0);
    return 0;
#else
    (void)rt;(void)enabled;return -1;
#endif
}
int m8rt_reset_interpreter_profile(m8rt *rt) {
#ifdef M8RT_INTERPRETER
    if(!rt || rt->in_emu || !rt->interpreter_profile) return -1;
    m8_native_interpreter_profile_reset(rt->interpreter_profile);return 0;
#else
    (void)rt;return -1;
#endif
}
int m8rt_dump_interpreter_profile(m8rt *rt,const char *path) {
#ifdef M8RT_INTERPRETER
    if(!rt || rt->in_emu || !rt->interpreter_profile || !path || !*path) return -1;
    return m8_native_interpreter_profile_dump(rt->interpreter_profile,path);
#else
    (void)rt;(void)path;return -1;
#endif
}
int m8rt_interpreter_profile_init(m8rt *rt) {
#ifdef M8RT_INTERPRETER
    const char *value=getenv("M8RT_NATIVE_INTERPRETER_PROFILE");
    bool enabled=value?(*value && strcmp(value,"0")):M8RT_INTERPRETER_PROFILE_DEFAULT;
    if(enabled && m8rt_set_interpreter_profile(rt,1)) return -1;
    if(enabled && value && strcmp(value,"1")) {
        rt->interpreter_profile_path=strdup(value);
        if(!rt->interpreter_profile_path) return -1;
    }
#else
    (void)rt;
#endif
    return 0;
}
void m8rt_interpreter_profile_destroy(m8rt *rt) {
#ifdef M8RT_INTERPRETER
    if(rt->interpreter_profile_path &&
       m8rt_dump_interpreter_profile(rt,rt->interpreter_profile_path))
        fprintf(stderr,"interpreter profile output failed: %s\n",rt->interpreter_profile_path);
    m8_native_interpreter_profile_free(rt->interpreter_profile);
    free(rt->interpreter_profile_path);
    rt->interpreter_profile=NULL;rt->interpreter_profile_path=NULL;
#else
    (void)rt;
#endif
}

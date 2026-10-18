#ifndef M8_HOST_NATIVE_BOOT_H
#define M8_HOST_NATIVE_BOOT_H
#include "native.h"
typedef struct m8_host_native_boot m8_host_native_boot;
#if M8EMU_LAB_REFERENCE
int m8_host_native_boot_choose(int cli,const char *environment,int target_default,
    char *error,size_t capacity);
#endif
m8_host_native_boot *m8_host_native_boot_preload(int enabled,const char *path,
    const m8_host_native *warm,const m8_host_aot *aot,char *error,size_t capacity);
int m8_host_native_boot_attach(m8rt *,m8_host_native_boot *,char *,size_t);
const char *m8_host_native_boot_path(const m8_host_native_boot *);
void m8_host_native_boot_close(m8_host_native_boot *);
int m8_host_native_boot_stats_json(FILE *,const m8rt_native_boot_app_stats *);
#endif

#include "native_boot.h"
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

struct m8_host_native_boot {
    SDL_SharedObject *library;
    char *path;
    m8rt_native_boot_app_config config;
    int enabled,attached,measure;
};
static void fail(char *error,size_t capacity,const char *format,...) {
    if(!error || !capacity) return;
    va_list args;va_start(args,format);vsnprintf(error,capacity,format,args);va_end(args);
}
int m8_host_native_boot_choose(int cli,const char *environment,int target_default,
    char *error,size_t capacity) {
    if(error && capacity) *error=0;
    if(cli==0 || cli==1) return cli;
    if(!environment) return target_default!=0;
    if(!strcmp(environment,"0")) return 0;
    if(!strcmp(environment,"1")) return 1;
    fail(error,capacity,"M8_NATIVE_BOOT requires 0 or 1: value=%s",environment);return -1;
}
static char *library_path(const char *selected) {
    if(selected) return SDL_strdup(selected);
    const char *environment=SDL_getenv("M8RT_NATIVE_BOOT_LIBRARY");
    if(environment) return SDL_strdup(environment);
    const char *base=SDL_GetBasePath();char *bundled=NULL;
    if(!base || SDL_asprintf(&bundled,"%slib/libm8native-boot" M8_LIBRARY_SUFFIX,base)<0) return NULL;
    SDL_PathInfo info;
    if(SDL_GetPathInfo(bundled,&info) && info.type==SDL_PATHTYPE_FILE) return bundled;
    char *portable=NULL;
    if(SDL_asprintf(&portable,"%slibs.aarch64/libm8native-boot" M8_LIBRARY_SUFFIX,base)>=0 &&
       SDL_GetPathInfo(portable,&info) && info.type==SDL_PATHTYPE_FILE) {
        SDL_free(bundled);return portable;
    }
    SDL_free(portable);return bundled;
}
m8_host_native_boot *m8_host_native_boot_preload(int enabled,const char *selected,
    const m8_host_native *warm,const m8_host_aot *aot,char *error,size_t capacity) {
    if(error && capacity) *error=0;
    m8_host_native_boot *boot=calloc(1,sizeof *boot);
    if(!boot) {fail(error,capacity,"Cannot allocate native boot loader");return NULL;}
    const char *measure=getenv("M8_NATIVE_BOOT_MEASURE");
    if(measure && strcmp(measure,"0") && strcmp(measure,"1")) {
        fail(error,capacity,"Boot measurement predicate=M8_NATIVE_BOOT_MEASURE value=%s expected=0_or_1",measure);goto failed;
    }
    boot->measure=measure && !strcmp(measure,"1");
    if(!enabled) return boot;
    if(!m8rt_native_boot_app_available()) {
        fail(error,capacity,"Native boot is unavailable in this runtime build");goto failed;
    }
    const m8rt_native_live_config *config=m8_host_native_config(warm);
    if(!config || m8_host_aot_native_dispatch(aot,&boot->config.lookup,
        &boot->config.entries,&boot->config.services)) {
        fail(error,capacity,"Native boot requires the qualified native engine and guarded AOT libraries");goto failed;
    }
    boot->config.warm=*config;
    boot->path=library_path(selected);
    if(!boot->path || !*boot->path) {fail(error,capacity,"Native boot requires a cold library path");goto failed;}
    boot->library=SDL_LoadObject(boot->path);
    if(!boot->library) {fail(error,capacity,"Cannot load native boot %s: %s",boot->path,SDL_GetError());goto failed;}
    uint32_t (*abi)(void)=(uint32_t (*)(void))SDL_LoadFunction(boot->library,"m8_native_boot_abi");
    const char *(*firmware)(void)=(const char *(*)(void))SDL_LoadFunction(boot->library,"m8_native_boot_firmware_sha256");
    if(!abi) {fail(error,capacity,"Native boot library predicate=abi_export value=0: path=%s",boot->path);goto failed;}
    uint32_t abi_value=abi();
    if(abi_value!=1u) {fail(error,capacity,"Native boot library predicate=abi value=%u expected=1: path=%s",abi_value,boot->path);goto failed;}
    if(!firmware) {fail(error,capacity,"Native boot library predicate=firmware_export value=0: path=%s",boot->path);goto failed;}
    const char *firmware_value=firmware();
    if(!firmware_value || strcmp(firmware_value,"20498b7637cee17baae249372db3b8377afd4e12a5e7062754c2ddd83621367f")) {
        fail(error,capacity,"Native boot library predicate=firmware value=%.64s: path=%s",firmware_value?firmware_value:"<null>",boot->path);goto failed;
    }
    boot->config.cold_step=(m8rt_native_step_fn)SDL_LoadFunction(boot->library,"m8_native_step");
    boot->config.cold_verify=(int (*)(m8_mem *,uint32_t *))SDL_LoadFunction(boot->library,"m8_native_boot_verify_memory");
    if(!boot->config.cold_step || !boot->config.cold_verify) {
        fail(error,capacity,"Native boot library lacks checked execution exports: path=%s",boot->path);goto failed;
    }
    boot->enabled=1;SDL_ClearError();return boot;
failed:
    m8_host_native_boot_close(boot);return NULL;
}
int m8_host_native_boot_attach(m8rt *rt,m8_host_native_boot *boot,char *error,size_t capacity) {
    if(error && capacity) *error=0;
    if(!boot || !rt || boot->attached || *m8rt_error(rt)) {
        fail(error,capacity,"Native boot attach requires a fresh idle runtime and an unused loader");return -1;
    }
    if(!boot->enabled) {
        if(boot->measure && m8rt_measure_native_boot_app(rt)) {
            fail(error,capacity,"Cannot observe reference boot at the native warm boundary");return -1;
        }
        boot->attached=1;return 0;
    }
    if(m8rt_set_native_boot_app(rt,&boot->config)) {
        fail(error,capacity,"Cannot admit native boot: %s",*m8rt_error(rt)?m8rt_error(rt):"runtime rejected configuration");return -1;
    }
    boot->attached=1;return 0;
}
const char *m8_host_native_boot_path(const m8_host_native_boot *boot) {return boot && boot->enabled?boot->path:NULL;}
void m8_host_native_boot_close(m8_host_native_boot *boot) {
    if(!boot) return;
    if(boot->library) SDL_UnloadObject(boot->library);
    SDL_free(boot->path);free(boot);
}
int m8_host_native_boot_stats_json(FILE *file,const m8rt_native_boot_app_stats *stats) {
    return fprintf(file,"{\"selected\":%d,\"warm\":%d,\"boot_elapsed_ns\":%" PRIu64
        ",\"boot_main_iters\":%" PRIu64 ",\"boot_cycles\":%" PRIu64 ",\"boot_pc\":%u}",
        stats->selected,stats->warm,stats->boot_elapsed_ns,stats->boot_main_iters,
        stats->boot_cycles,stats->boot_pc)<0?-1:0;
}

#include "profiling.h"
#include "pc-profile.h"
#include "host-pmu.h"
#include <errno.h>

int m8_app_profile_start(void *context, char *error, size_t size) {
    m8_app_profile *p = context;
    /* Both timers and perf events must belong to the guest owner. Prepare
     * after audio-thread creation; SIGEV_THREAD_ID targets only this thread. */
    if (m8_profile_prepare(p->hz) || m8_pmu_prepare(p->pmu, p->sample) ||
        m8_profile_start(p->aot) || m8_pmu_start()) {
        int saved = errno;
        m8_profile_destroy(); m8_pmu_destroy();
        snprintf(error, size, "Guest profile start failed: error=%d", saved);
        return -1;
    }
    return 0;
}
void m8_app_profile_stop(void *context) {
    (void)context;
    m8_pmu_stop(); m8_profile_stop();
}
int m8_app_profile_finish(void *context, char *error, size_t size) {
    m8_app_profile *p = context;
    fputs("{\"schema\":\"m8-native-profile/1\",\"thread\":\"guest\",\"profile\":", p->output);
    m8_profile_write_json(p->output);
    fputs(",\"host_pmu\":", p->output);
    m8_pmu_write_json(p->output, p->aot);
    fputs("}\n", p->output);
    int failed = fflush(p->output) || ferror(p->output);
    m8_profile_destroy(); m8_pmu_destroy();
    if (failed) snprintf(error, size, "Guest profile write failed: error=%d", errno);
    return failed ? -1 : 0;
}

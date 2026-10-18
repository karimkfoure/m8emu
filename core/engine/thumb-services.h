#ifndef M8_THUMB_SERVICES_H
#define M8_THUMB_SERVICES_H
#include <m8emu/m8rt.h>

/* Consume the last portable interpreter result on the execution thread.
 * Call after every result, including completed WFI. Return 0 to continue or
 * stop the current slice according to native.stopped; otherwise return a CPU
 * backend error number. FP_CONTEXT returns 0 at the unchanged PC: retry once
 * with m8_thumb_interpret_prepared before another service call. */
int m8rt_native_interpreter_service(m8rt *rt, int result);
#endif

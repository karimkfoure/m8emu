#ifndef M8_THREAD_POLICY_H
#define M8_THREAD_POLICY_H

#include <stddef.h>

/* Bind only the caller to a CSV list of CPU numbers. The first call saves
 * its original allowed mask. Call at startup, main first and then guest;
 * concurrent initialization is not supported. No priority is changed. */
int m8_thread_policy_bind(const char *cpus, char *error, size_t capacity);
/* Request latency-critical scheduling for the caller. On macOS this selects
 * the user-interactive QoS class, which keeps a mostly-sleeping guest thread
 * on performance cores. Other hosts keep their scheduler default. */
int m8_thread_policy_interactive(char *error, size_t capacity);

#endif

#ifndef M8_NATIVE_STATIC_H
#define M8_NATIVE_STATIC_H
#include "native_regions.h"
#include "native_step.h"
#if defined(__GNUC__)
#define M8_NATIVE_STATIC_EXPORT __attribute__((visibility("default")))
#else
#define M8_NATIVE_STATIC_EXPORT
#endif

/* Generated, default-off companion. The runtime owns service and code guards. */
M8_NATIVE_STATIC_EXPORT uint32_t m8_native_static_abi(void);
M8_NATIVE_STATIC_EXPORT const char *m8_native_static_firmware_sha256(void);
M8_NATIVE_STATIC_EXPORT int m8_native_static_step(m8_cpu *, m8_mem *, m8_native_arch *);
M8_NATIVE_STATIC_EXPORT int m8_native_static_region_step(m8_cpu *, m8_mem *, m8_native_arch *);
M8_NATIVE_STATIC_EXPORT int m8_native_static_region_verify_memory(m8_mem *, uint32_t *);
/* Reset admission uses flash only. ITCM verification requires the boot copy. */
M8_NATIVE_STATIC_EXPORT int m8_native_static_region_verify_flash(m8_mem *, uint32_t *);
M8_NATIVE_STATIC_EXPORT int m8_native_static_region_verify_itcm(m8_mem *, uint32_t *);
#undef M8_NATIVE_STATIC_EXPORT
#endif

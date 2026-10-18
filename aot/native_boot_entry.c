#include "native_step.h"

/* Unique link names keep product admission independent of dlopen order. */
__attribute__((visibility("default")))
int m8_native_cold_entry(m8_cpu *cpu,m8_mem *mem,m8_native_arch *arch) {
    return m8_native_step(cpu,mem,arch);
}

#ifndef M8_CPU_BACKEND_H
#define M8_CPU_BACKEND_H
#include <m8emu/registers.h>
#include "native_step.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Native-owned state and mapping services. This module does not execute guest
 * instructions. The runtime owns execution, callback delivery and clocks. */
typedef struct m8_cpu_backend m8_cpu_backend;
typedef struct m8_cpu_backend_context m8_cpu_backend_context;
typedef uintptr_t m8_cpu_backend_hook;
typedef enum m8_cpu_backend_error {
    M8_CPU_BACKEND_ERR_OK = 0,
    M8_CPU_BACKEND_ERR_NOMEM = 1,
    M8_CPU_BACKEND_ERR_ARCH = 2,
    M8_CPU_BACKEND_ERR_HANDLE = 3,
    M8_CPU_BACKEND_ERR_MODE = 4,
    M8_CPU_BACKEND_ERR_VERSION = 5,
    M8_CPU_BACKEND_ERR_READ_UNMAPPED = 6,
    M8_CPU_BACKEND_ERR_WRITE_UNMAPPED = 7,
    M8_CPU_BACKEND_ERR_FETCH_UNMAPPED = 8,
    M8_CPU_BACKEND_ERR_HOOK = 9,
    M8_CPU_BACKEND_ERR_INSN_INVALID = 10,
    M8_CPU_BACKEND_ERR_MAP = 11,
    M8_CPU_BACKEND_ERR_WRITE_PROT = 12,
    M8_CPU_BACKEND_ERR_READ_PROT = 13,
    M8_CPU_BACKEND_ERR_FETCH_PROT = 14,
    M8_CPU_BACKEND_ERR_ARG = 15,
    M8_CPU_BACKEND_ERR_READ_UNALIGNED = 16,
    M8_CPU_BACKEND_ERR_WRITE_UNALIGNED = 17,
    M8_CPU_BACKEND_ERR_FETCH_UNALIGNED = 18,
    M8_CPU_BACKEND_ERR_HOOK_EXIST = 19,
    M8_CPU_BACKEND_ERR_RESOURCE = 20,
    M8_CPU_BACKEND_ERR_EXCEPTION = 21,
    M8_CPU_BACKEND_ERR_OVERFLOW = 22,
    M8_CPU_BACKEND_ERR_UNSUPPORTED = 256
} m8_cpu_backend_error;
typedef enum m8_cpu_backend_memory_type {
    M8_CPU_BACKEND_MEM_READ = 16,
    M8_CPU_BACKEND_MEM_WRITE = 17,
    M8_CPU_BACKEND_MEM_FETCH = 18,
    M8_CPU_BACKEND_MEM_READ_UNMAPPED = 19,
    M8_CPU_BACKEND_MEM_WRITE_UNMAPPED = 20,
    M8_CPU_BACKEND_MEM_FETCH_UNMAPPED = 21,
    M8_CPU_BACKEND_MEM_WRITE_PROT = 22,
    M8_CPU_BACKEND_MEM_READ_PROT = 23,
    M8_CPU_BACKEND_MEM_FETCH_PROT = 24,
    M8_CPU_BACKEND_MEM_READ_AFTER = 25,
} m8_cpu_backend_memory_type;
enum {
    M8_CPU_BACKEND_ARCH_ARM = 1,
    M8_CPU_BACKEND_MODE_THUMB = 16,
    M8_CPU_BACKEND_CPU_CORTEX_M7 = 10,
    M8_CPU_BACKEND_PROT_NONE = 0, M8_CPU_BACKEND_PROT_READ = 1,
    M8_CPU_BACKEND_PROT_WRITE = 2, M8_CPU_BACKEND_PROT_EXEC = 4,
    M8_CPU_BACKEND_PROT_ALL = 7,
    M8_CPU_BACKEND_HOOK_INTR = 1,
    M8_CPU_BACKEND_HOOK_CODE = 4, M8_CPU_BACKEND_HOOK_BLOCK = 8,
    M8_CPU_BACKEND_HOOK_MEM_READ_UNMAPPED = 16,
    M8_CPU_BACKEND_HOOK_MEM_WRITE_UNMAPPED = 32,
    M8_CPU_BACKEND_HOOK_MEM_FETCH_UNMAPPED = 64,
    M8_CPU_BACKEND_HOOK_MEM_READ_PROT = 128,
    M8_CPU_BACKEND_HOOK_MEM_WRITE_PROT = 256,
    M8_CPU_BACKEND_HOOK_MEM_FETCH_PROT = 512,
    M8_CPU_BACKEND_HOOK_MEM_READ = 1024,
    M8_CPU_BACKEND_HOOK_MEM_WRITE = 2048,
    M8_CPU_BACKEND_HOOK_MEM_FETCH = 4096,
    M8_CPU_BACKEND_HOOK_MEM_UNMAPPED = 112,
    M8_CPU_BACKEND_HOOK_MEM_PROT = 896
};
typedef uint64_t (*m8_cpu_backend_mmio_read_fn)(m8_cpu_backend *,uint64_t,unsigned,void *);
typedef void (*m8_cpu_backend_mmio_write_fn)(m8_cpu_backend *,uint64_t,unsigned,uint64_t,void *);

m8_cpu_backend_error m8_cpu_backend_open(int arch,int mode,m8_cpu_backend **out);
m8_cpu_backend_error m8_cpu_backend_close(m8_cpu_backend *);
unsigned m8_cpu_backend_version(unsigned *major,unsigned *minor);
const char *m8_cpu_backend_strerror(m8_cpu_backend_error);
/* External state remains the owner before and after execution is enabled.
 * Bind once before maps or saved contexts. Pointers must outlive the backend. */
m8_cpu_backend_error m8_cpu_backend_bind_state(m8_cpu_backend *,m8_cpu *,m8_native_arch *,uint32_t *cpsr);
size_t m8_cpu_backend_register_size(int id);
m8_cpu_backend_error m8_cpu_backend_reg_read(m8_cpu_backend *,int id,void *out);
m8_cpu_backend_error m8_cpu_backend_reg_write(m8_cpu_backend *,int id,const void *value);
m8_cpu_backend_error m8_cpu_backend_reg_read_batch(m8_cpu_backend *,const int *,void *const *,int count);
m8_cpu_backend_error m8_cpu_backend_reg_write_batch(m8_cpu_backend *,const int *,void *const *,int count);
m8_cpu_backend_error m8_cpu_backend_context_alloc(m8_cpu_backend *,m8_cpu_backend_context **);
m8_cpu_backend_error m8_cpu_backend_context_free(m8_cpu_backend_context *);
m8_cpu_backend_error m8_cpu_backend_context_save(m8_cpu_backend *,m8_cpu_backend_context *);
m8_cpu_backend_error m8_cpu_backend_context_restore(m8_cpu_backend *,const m8_cpu_backend_context *);

/* Maps use a 4 KiB page and a 32-bit guest address space. Host access copies
 * mapped RAM bytes without guest permissions or MMIO side effects. Bus access
 * checks permissions and invokes at most one MMIO callback for widths 1/2/4/8. */
m8_cpu_backend_error m8_cpu_backend_mem_map(m8_cpu_backend *,uint64_t,size_t,unsigned);
m8_cpu_backend_error m8_cpu_backend_mem_map_ptr(m8_cpu_backend *,uint64_t,size_t,unsigned,void *);
m8_cpu_backend_error m8_cpu_backend_mmio_map(m8_cpu_backend *,uint64_t,size_t,m8_cpu_backend_mmio_read_fn,void *,m8_cpu_backend_mmio_write_fn,void *);
m8_cpu_backend_error m8_cpu_backend_mem_unmap(m8_cpu_backend *,uint64_t,size_t);
m8_cpu_backend_error m8_cpu_backend_mem_protect(m8_cpu_backend *,uint64_t,size_t,unsigned);
m8_cpu_backend_error m8_cpu_backend_mem_read(m8_cpu_backend *,uint64_t,void *,size_t);
m8_cpu_backend_error m8_cpu_backend_mem_write(m8_cpu_backend *,uint64_t,const void *,size_t);
m8_cpu_backend_error m8_cpu_backend_bus_read(m8_cpu_backend *,uint64_t,unsigned,uint64_t *);
m8_cpu_backend_error m8_cpu_backend_bus_write(m8_cpu_backend *,uint64_t,unsigned,uint64_t);
m8_cpu_backend_error m8_cpu_backend_fetch(m8_cpu_backend *,uint64_t,void *,size_t);
const uint64_t *m8_cpu_backend_memory_generation(m8_cpu_backend *);

/* These retain reference hook registrations only. Native execution must deliver
 * its own runtime events. Unknown hook types are rejected. No hook is executed
 * by this module, including during host memory access. */
m8_cpu_backend_error m8_cpu_backend_hook_add(m8_cpu_backend *,m8_cpu_backend_hook *,int,void *,void *,uint64_t,uint64_t,...);
m8_cpu_backend_error m8_cpu_backend_hook_del(m8_cpu_backend *,m8_cpu_backend_hook);
m8_cpu_backend_error m8_cpu_backend_ctl_set_cpu_model(m8_cpu_backend *,int);
m8_cpu_backend_error m8_cpu_backend_ctl_set_tcg_buffer_size(m8_cpu_backend *,uint32_t);
m8_cpu_backend_error m8_cpu_backend_ctl_flush_tb(m8_cpu_backend *);
m8_cpu_backend_error m8_cpu_backend_ctl_remove_cache(m8_cpu_backend *,uint64_t,uint64_t);
unsigned m8_cpu_backend_arm_exclusive_monitor_abi(void);
/* Stop uses an atomic flag; the caller clears it before each execution slice.
 * It never writes CPU state or a runtime's non-atomic stopped field. */
m8_cpu_backend_error m8_cpu_backend_emu_stop(m8_cpu_backend *);
bool m8_cpu_backend_stop_requested(const m8_cpu_backend *);
void m8_cpu_backend_clear_stop(m8_cpu_backend *);
m8_cpu_backend_error m8_cpu_backend_emu_start(m8_cpu_backend *,uint64_t,uint64_t,uint64_t,size_t);

/* Private source compatibility, with no legacy link symbols or headers. */
typedef m8_cpu_backend uc_engine;
typedef m8_cpu_backend_context uc_context;
typedef m8_cpu_backend_error uc_err;
typedef m8_cpu_backend_hook uc_hook;
typedef m8_cpu_backend_memory_type uc_mem_type;
#define UC_ERR_OK M8_CPU_BACKEND_ERR_OK
#define UC_ERR_NOMEM M8_CPU_BACKEND_ERR_NOMEM
#define UC_ERR_ARCH M8_CPU_BACKEND_ERR_ARCH
#define UC_ERR_HANDLE M8_CPU_BACKEND_ERR_HANDLE
#define UC_ERR_MODE M8_CPU_BACKEND_ERR_MODE
#define UC_ERR_VERSION M8_CPU_BACKEND_ERR_VERSION
#define UC_ERR_READ_UNMAPPED M8_CPU_BACKEND_ERR_READ_UNMAPPED
#define UC_ERR_WRITE_UNMAPPED M8_CPU_BACKEND_ERR_WRITE_UNMAPPED
#define UC_ERR_FETCH_UNMAPPED M8_CPU_BACKEND_ERR_FETCH_UNMAPPED
#define UC_ERR_HOOK M8_CPU_BACKEND_ERR_HOOK
#define UC_ERR_INSN_INVALID M8_CPU_BACKEND_ERR_INSN_INVALID
#define UC_ERR_MAP M8_CPU_BACKEND_ERR_MAP
#define UC_ERR_WRITE_PROT M8_CPU_BACKEND_ERR_WRITE_PROT
#define UC_ERR_READ_PROT M8_CPU_BACKEND_ERR_READ_PROT
#define UC_ERR_FETCH_PROT M8_CPU_BACKEND_ERR_FETCH_PROT
#define UC_ERR_ARG M8_CPU_BACKEND_ERR_ARG
#define UC_ERR_READ_UNALIGNED M8_CPU_BACKEND_ERR_READ_UNALIGNED
#define UC_ERR_WRITE_UNALIGNED M8_CPU_BACKEND_ERR_WRITE_UNALIGNED
#define UC_ERR_FETCH_UNALIGNED M8_CPU_BACKEND_ERR_FETCH_UNALIGNED
#define UC_ERR_HOOK_EXIST M8_CPU_BACKEND_ERR_HOOK_EXIST
#define UC_ERR_RESOURCE M8_CPU_BACKEND_ERR_RESOURCE
#define UC_ERR_EXCEPTION M8_CPU_BACKEND_ERR_EXCEPTION
#define UC_ERR_OVERFLOW M8_CPU_BACKEND_ERR_OVERFLOW
#define UC_MEM_READ M8_CPU_BACKEND_MEM_READ
#define UC_MEM_WRITE M8_CPU_BACKEND_MEM_WRITE
#define UC_MEM_FETCH M8_CPU_BACKEND_MEM_FETCH
#define UC_MEM_READ_UNMAPPED M8_CPU_BACKEND_MEM_READ_UNMAPPED
#define UC_MEM_WRITE_UNMAPPED M8_CPU_BACKEND_MEM_WRITE_UNMAPPED
#define UC_MEM_FETCH_UNMAPPED M8_CPU_BACKEND_MEM_FETCH_UNMAPPED
#define UC_MEM_WRITE_PROT M8_CPU_BACKEND_MEM_WRITE_PROT
#define UC_MEM_READ_PROT M8_CPU_BACKEND_MEM_READ_PROT
#define UC_MEM_FETCH_PROT M8_CPU_BACKEND_MEM_FETCH_PROT
#define UC_MEM_READ_AFTER M8_CPU_BACKEND_MEM_READ_AFTER
#define UC_ARCH_ARM M8_CPU_BACKEND_ARCH_ARM
#define UC_MODE_THUMB M8_CPU_BACKEND_MODE_THUMB
#define UC_CPU_ARM_CORTEX_M7 M8_CPU_BACKEND_CPU_CORTEX_M7
#define UC_PROT_NONE M8_CPU_BACKEND_PROT_NONE
#define UC_PROT_READ M8_CPU_BACKEND_PROT_READ
#define UC_PROT_WRITE M8_CPU_BACKEND_PROT_WRITE
#define UC_PROT_EXEC M8_CPU_BACKEND_PROT_EXEC
#define UC_PROT_ALL M8_CPU_BACKEND_PROT_ALL
#define UC_HOOK_INTR M8_CPU_BACKEND_HOOK_INTR
#define UC_HOOK_CODE M8_CPU_BACKEND_HOOK_CODE
#define UC_HOOK_BLOCK M8_CPU_BACKEND_HOOK_BLOCK
#define UC_HOOK_MEM_READ_UNMAPPED M8_CPU_BACKEND_HOOK_MEM_READ_UNMAPPED
#define UC_HOOK_MEM_WRITE_UNMAPPED M8_CPU_BACKEND_HOOK_MEM_WRITE_UNMAPPED
#define UC_HOOK_MEM_FETCH_UNMAPPED M8_CPU_BACKEND_HOOK_MEM_FETCH_UNMAPPED
#define UC_HOOK_MEM_READ_PROT M8_CPU_BACKEND_HOOK_MEM_READ_PROT
#define UC_HOOK_MEM_WRITE_PROT M8_CPU_BACKEND_HOOK_MEM_WRITE_PROT
#define UC_HOOK_MEM_FETCH_PROT M8_CPU_BACKEND_HOOK_MEM_FETCH_PROT
#define UC_HOOK_MEM_READ M8_CPU_BACKEND_HOOK_MEM_READ
#define UC_HOOK_MEM_WRITE M8_CPU_BACKEND_HOOK_MEM_WRITE
#define UC_HOOK_MEM_FETCH M8_CPU_BACKEND_HOOK_MEM_FETCH
#define UC_HOOK_MEM_UNMAPPED M8_CPU_BACKEND_HOOK_MEM_UNMAPPED
#define UC_HOOK_MEM_PROT M8_CPU_BACKEND_HOOK_MEM_PROT
#define UC_ARM_REG_APSR M8RT_REG_APSR
#define UC_ARM_REG_APSR_NZCV M8RT_REG_APSR_NZCV
#define UC_ARM_REG_CPSR M8RT_REG_CPSR
#define UC_ARM_REG_FPEXC M8RT_REG_FPEXC
#define UC_ARM_REG_FPSCR M8RT_REG_FPSCR
#define UC_ARM_REG_FPSCR_NZCV M8RT_REG_FPSCR_NZCV
#define UC_ARM_REG_ITSTATE M8RT_REG_ITSTATE
#define UC_ARM_REG_LR M8RT_REG_LR
#define UC_ARM_REG_PC M8RT_REG_PC
#define UC_ARM_REG_SP M8RT_REG_SP
#define UC_ARM_REG_D0 M8RT_REG_D0
#define UC_ARM_REG_D1 M8RT_REG_D1
#define UC_ARM_REG_D2 M8RT_REG_D2
#define UC_ARM_REG_D3 M8RT_REG_D3
#define UC_ARM_REG_D4 M8RT_REG_D4
#define UC_ARM_REG_D5 M8RT_REG_D5
#define UC_ARM_REG_D6 M8RT_REG_D6
#define UC_ARM_REG_D7 M8RT_REG_D7
#define UC_ARM_REG_D8 M8RT_REG_D8
#define UC_ARM_REG_D9 M8RT_REG_D9
#define UC_ARM_REG_D10 M8RT_REG_D10
#define UC_ARM_REG_D11 M8RT_REG_D11
#define UC_ARM_REG_D12 M8RT_REG_D12
#define UC_ARM_REG_D13 M8RT_REG_D13
#define UC_ARM_REG_D14 M8RT_REG_D14
#define UC_ARM_REG_D15 M8RT_REG_D15
#define UC_ARM_REG_D16 M8RT_REG_D16
#define UC_ARM_REG_D17 M8RT_REG_D17
#define UC_ARM_REG_D18 M8RT_REG_D18
#define UC_ARM_REG_D19 M8RT_REG_D19
#define UC_ARM_REG_D20 M8RT_REG_D20
#define UC_ARM_REG_D21 M8RT_REG_D21
#define UC_ARM_REG_D22 M8RT_REG_D22
#define UC_ARM_REG_D23 M8RT_REG_D23
#define UC_ARM_REG_D24 M8RT_REG_D24
#define UC_ARM_REG_D25 M8RT_REG_D25
#define UC_ARM_REG_D26 M8RT_REG_D26
#define UC_ARM_REG_D27 M8RT_REG_D27
#define UC_ARM_REG_D28 M8RT_REG_D28
#define UC_ARM_REG_D29 M8RT_REG_D29
#define UC_ARM_REG_D30 M8RT_REG_D30
#define UC_ARM_REG_D31 M8RT_REG_D31
#define UC_ARM_REG_R0 M8RT_REG_R0
#define UC_ARM_REG_R1 M8RT_REG_R1
#define UC_ARM_REG_R2 M8RT_REG_R2
#define UC_ARM_REG_R3 M8RT_REG_R3
#define UC_ARM_REG_R4 M8RT_REG_R4
#define UC_ARM_REG_R5 M8RT_REG_R5
#define UC_ARM_REG_R6 M8RT_REG_R6
#define UC_ARM_REG_R7 M8RT_REG_R7
#define UC_ARM_REG_R8 M8RT_REG_R8
#define UC_ARM_REG_R9 M8RT_REG_R9
#define UC_ARM_REG_R10 M8RT_REG_R10
#define UC_ARM_REG_R11 M8RT_REG_R11
#define UC_ARM_REG_R12 M8RT_REG_R12
#define UC_ARM_REG_R13 M8RT_REG_R13
#define UC_ARM_REG_R14 M8RT_REG_R14
#define UC_ARM_REG_R15 M8RT_REG_R15
#define UC_ARM_REG_S0 M8RT_REG_S0
#define UC_ARM_REG_S1 M8RT_REG_S1
#define UC_ARM_REG_S2 M8RT_REG_S2
#define UC_ARM_REG_S3 M8RT_REG_S3
#define UC_ARM_REG_S4 M8RT_REG_S4
#define UC_ARM_REG_S5 M8RT_REG_S5
#define UC_ARM_REG_S6 M8RT_REG_S6
#define UC_ARM_REG_S7 M8RT_REG_S7
#define UC_ARM_REG_S8 M8RT_REG_S8
#define UC_ARM_REG_S9 M8RT_REG_S9
#define UC_ARM_REG_S10 M8RT_REG_S10
#define UC_ARM_REG_S11 M8RT_REG_S11
#define UC_ARM_REG_S12 M8RT_REG_S12
#define UC_ARM_REG_S13 M8RT_REG_S13
#define UC_ARM_REG_S14 M8RT_REG_S14
#define UC_ARM_REG_S15 M8RT_REG_S15
#define UC_ARM_REG_S16 M8RT_REG_S16
#define UC_ARM_REG_S17 M8RT_REG_S17
#define UC_ARM_REG_S18 M8RT_REG_S18
#define UC_ARM_REG_S19 M8RT_REG_S19
#define UC_ARM_REG_S20 M8RT_REG_S20
#define UC_ARM_REG_S21 M8RT_REG_S21
#define UC_ARM_REG_S22 M8RT_REG_S22
#define UC_ARM_REG_S23 M8RT_REG_S23
#define UC_ARM_REG_S24 M8RT_REG_S24
#define UC_ARM_REG_S25 M8RT_REG_S25
#define UC_ARM_REG_S26 M8RT_REG_S26
#define UC_ARM_REG_S27 M8RT_REG_S27
#define UC_ARM_REG_S28 M8RT_REG_S28
#define UC_ARM_REG_S29 M8RT_REG_S29
#define UC_ARM_REG_S30 M8RT_REG_S30
#define UC_ARM_REG_S31 M8RT_REG_S31
#define UC_ARM_REG_IPSR M8RT_REG_IPSR
#define UC_ARM_REG_MSP M8RT_REG_MSP
#define UC_ARM_REG_PSP M8RT_REG_PSP
#define UC_ARM_REG_CONTROL M8RT_REG_CONTROL
#define UC_ARM_REG_IAPSR M8RT_REG_IAPSR
#define UC_ARM_REG_EAPSR M8RT_REG_EAPSR
#define UC_ARM_REG_XPSR M8RT_REG_XPSR
#define UC_ARM_REG_EPSR M8RT_REG_EPSR
#define UC_ARM_REG_IEPSR M8RT_REG_IEPSR
#define UC_ARM_REG_PRIMASK M8RT_REG_PRIMASK
#define UC_ARM_REG_BASEPRI M8RT_REG_BASEPRI
#define UC_ARM_REG_BASEPRI_MAX M8RT_REG_BASEPRI_MAX
#define UC_ARM_REG_FAULTMASK M8RT_REG_FAULTMASK
#define UC_ARM_REG_APSR_NZCVQ M8RT_REG_APSR_NZCVQ
#define UC_ARM_REG_APSR_G M8RT_REG_APSR_G
#define UC_ARM_REG_APSR_NZCVQG M8RT_REG_APSR_NZCVQG
#define UC_ARM_REG_IAPSR_NZCVQ M8RT_REG_IAPSR_NZCVQ
#define UC_ARM_REG_IAPSR_G M8RT_REG_IAPSR_G
#define UC_ARM_REG_IAPSR_NZCVQG M8RT_REG_IAPSR_NZCVQG
#define UC_ARM_REG_EAPSR_NZCVQ M8RT_REG_EAPSR_NZCVQ
#define UC_ARM_REG_EAPSR_G M8RT_REG_EAPSR_G
#define UC_ARM_REG_EAPSR_NZCVQG M8RT_REG_EAPSR_NZCVQG
#define UC_ARM_REG_XPSR_NZCVQ M8RT_REG_XPSR_NZCVQ
#define UC_ARM_REG_XPSR_G M8RT_REG_XPSR_G
#define UC_ARM_REG_XPSR_NZCVQG M8RT_REG_XPSR_NZCVQG
#define UC_ARM_REG_EXCLUSIVE_ADDR M8RT_REG_EXCLUSIVE_ADDR
#define UC_ARM_REG_EXCLUSIVE_VAL M8RT_REG_EXCLUSIVE_VAL
#define UC_ARM_REG_EXCLUSIVE_HIGH M8RT_REG_EXCLUSIVE_HIGH
#define UC_ARM_REG_SB M8RT_REG_SB
#define UC_ARM_REG_SL M8RT_REG_SL
#define UC_ARM_REG_FP M8RT_REG_FP
#define UC_ARM_REG_IP M8RT_REG_IP
#define uc_open m8_cpu_backend_open
#define uc_close m8_cpu_backend_close
#define uc_version m8_cpu_backend_version
#define uc_strerror m8_cpu_backend_strerror
#define uc_reg_read m8_cpu_backend_reg_read
#define uc_reg_write m8_cpu_backend_reg_write
#define uc_reg_read_batch m8_cpu_backend_reg_read_batch
#define uc_reg_write_batch m8_cpu_backend_reg_write_batch
#define uc_mem_map m8_cpu_backend_mem_map
#define uc_mem_map_ptr m8_cpu_backend_mem_map_ptr
#define uc_mmio_map m8_cpu_backend_mmio_map
#define uc_mem_read m8_cpu_backend_mem_read
#define uc_mem_write m8_cpu_backend_mem_write
#define uc_mem_unmap m8_cpu_backend_mem_unmap
#define uc_mem_protect m8_cpu_backend_mem_protect
#define uc_context_alloc m8_cpu_backend_context_alloc
#define uc_context_free m8_cpu_backend_context_free
#define uc_context_save m8_cpu_backend_context_save
#define uc_context_restore m8_cpu_backend_context_restore
#define uc_hook_add m8_cpu_backend_hook_add
#define uc_hook_del m8_cpu_backend_hook_del
#define uc_emu_start m8_cpu_backend_emu_start
#define uc_emu_stop m8_cpu_backend_emu_stop
#define uc_ctl_set_cpu_model m8_cpu_backend_ctl_set_cpu_model
#define uc_ctl_set_tcg_buffer_size m8_cpu_backend_ctl_set_tcg_buffer_size
#define uc_ctl_flush_tb m8_cpu_backend_ctl_flush_tb
#define uc_ctl_remove_cache m8_cpu_backend_ctl_remove_cache
#define uc_m8_memory_generation m8_cpu_backend_memory_generation
#define uc_m8_arm_exclusive_monitor_abi m8_cpu_backend_arm_exclusive_monitor_abi
#endif

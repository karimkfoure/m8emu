#ifndef M8_THUMB_INTERPRETER_H
#define M8_THUMB_INTERPRETER_H
#include "native_step.h"

/* Decode one instruction from live mapped bytes. Return the native-step ABI:
 * 0 completed, 1 fault (retain completed effects), 2 unsupported before effects.
 * Invalid-state branches publish the target PC and clear T before status 1,
 * as the reference engine does. Other faults retain the instruction PC.
 * The caller supplies execute permissions, clocks, and exception delivery. */
int m8_thumb_interpret(m8_cpu *cpu, m8_mem *mem, m8_native_arch *arch);

/* Fetch must copy exactly size bytes without data-bus effects. Return zero
 * on success; any other result is a fetch fault. This permits ROM and a
 * runtime execute-permission check without extending the AOT memory ABI. */
typedef int (*m8_thumb_fetch_fn)(void *ctx, uint32_t pc, unsigned size,
                               uint8_t *bytes);
int m8_thumb_interpret_fetch(m8_cpu *cpu, m8_mem *mem, m8_native_arch *arch,
                            m8_thumb_fetch_fn fetch, void *ctx);

/* Retry after the runtime has resolved FP_CONTEXT. This bypasses only the
 * inactive FPCA request, for runtimes whose prepared context keeps FPCA clear
 * (for example ASPEN=0). A NULL fetch selects live mapped spans. */
int m8_thumb_interpret_prepared(m8_cpu *cpu, m8_mem *mem, m8_native_arch *arch,
                              m8_thumb_fetch_fn fetch, void *ctx);

typedef struct m8_thumb_result {
    uint32_t pc;
    uint32_t opcode; /* first halfword in bits 31:16 for a wide instruction */
    uint32_t width;
    int status;
    const char *family;
    uint32_t reason;
    uint32_t address, size, write; /* first failed data access, when applicable */
    uint32_t next_pc; /* completed successor or invalid-state branch target */
} m8_thumb_result;

enum m8_thumb_reason {
    M8_THUMB_REASON_NONE, M8_THUMB_REASON_FETCH, M8_THUMB_REASON_MEMORY,
    M8_THUMB_REASON_STATE, M8_THUMB_REASON_UNDEFINED, M8_THUMB_REASON_SVC,
    M8_THUMB_REASON_BREAKPOINT, M8_THUMB_REASON_FP_CONTEXT,
    M8_THUMB_REASON_UNSUPPORTED, M8_THUMB_REASON_WAIT,
    M8_THUMB_REASON_EVENT, M8_THUMB_REASON_YIELD, M8_THUMB_REASON_NOCP
};

/* Execution-thread diagnostic; valid until the next interpreter call. */
const m8_thumb_result *m8_thumb_last_result(void);

enum m8_thumb_tb_boundary {
    M8_THUMB_TB_CONTINUE = 0,
    M8_THUMB_TB_BRANCH = 1u,
    M8_THUMB_TB_CONTROL = 2u,
    M8_THUMB_TB_ISB = 4u,
    M8_THUMB_TB_EXCEPTION = 8u,
    M8_THUMB_TB_HINT = 16u
};

/* Semantic translation-block boundaries for supported ARMv7E-M encodings.
 * This is not an encoding validator or a TCG operation-count estimator.
 * Branches end a block even if their condition fails. CONTROL/IPSR are
 * the pre-instruction values (unprivileged CPS is a NOP). Page, op-buffer,
 * breakpoint, hook-stop and external exception boundaries belong to the
 * caller's trusted block map and execution owner. */
unsigned m8_thumb_decode_boundary(uint32_t opcode, unsigned width,
                                  uint32_t control, uint32_t ipsr);
unsigned m8_thumb_last_boundary(void);
#endif

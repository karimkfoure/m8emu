#include "internal.h"
#include "storage.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>

#define SD_LO UINT32_C(0x402c0000)
#define SD_HI UINT32_C(0x402c8000)
#define SD_CONTROLLERS 8
#define TRACE_CAP 5000
#define INT_CC UINT32_C(1)
#define INT_TC UINT32_C(2)
#define INT_BRR UINT32_C(32)
#define INT_CCE (UINT32_C(1) << 17)
#define INT_DCE (UINT32_C(1) << 21)
#define INT_ERRORS ((UINT32_C(0x7f) << 16) | (UINT32_C(1) << 24) | (UINT32_C(1) << 28))
#define MIX_DMAEN UINT32_C(1)

typedef struct {
    bool valid, cmd6, infinite, dma;
    size_t ptr, start_ptr;
    int64_t blocks_left;
    uint32_t words_left, words_per_block, cmd, arg, blksize, blkcnt, xfertyp;
    uint64_t words_consumed, bytes_accepted, write_id;
    /* 0 pending, 1 completed, 2 cancelled. */
    unsigned history_status;
} sd_transfer;
typedef struct {
    bool valid, expected_infinite;
    unsigned event;
    uint32_t base, cmd, arg;
    uint64_t write_id, bytes_expected, bytes_accepted;
    char reason[64];
} sd_event;
typedef struct {
    bool present, write, ok, dmaen, switch_status, completion_valid;
    uint32_t ds_addr, dmas, cmd, arg;
    size_t nbytes;
    uint64_t write_id;
    char error[160];
    uint8_t payload[64];
    sd_event completion;
} sd_dma;
typedef struct {
    bool app, block_set, guard, fifo_fault, error_idle;
    uint32_t blksize, blkcnt, guard_cmd, guard_arg;
    bool present[1024];
    sd_transfer read, write;
    sd_dma dma;
} sd_controller;
struct m8rt_sd {
    uint8_t *image;
    size_t image_size;
    FILE *private_file;
    sd_controller controller[SD_CONTROLLERS];
    uint64_t pio_words, started, completed, cancelled, errors, pending, bytes_accepted, image_revision;
    bool read_only, fault_armed;
    uint64_t fault_remaining;
    sd_event first_cancelled, first_error, last;
    uint32_t trace[TRACE_CAP][2];
    size_t trace_len;
};

static sd_controller *controller(m8rt *rt, uint32_t base)
{
    return &rt->sd->controller[(base - SD_LO) >> 12];
}
static void sd_raw_write(m8rt *rt, uint32_t addr, uint32_t value)
{
    controller(rt, addr & ~UINT32_C(0xfff))->present[(addr & 0xfff) >> 2] = true;
    m8rt_raw_write(rt, addr, value);
}
static int controller_irq(uint32_t base)
{
    return base == SD_LO ? 110 : base == SD_LO + 0x4000 ? 111 : -1;
}
static bool transfer_pending(const sd_transfer *st)
{
    return st->valid && (st->infinite || st->blocks_left > 0 || st->words_left > 0);
}
static bool irq_live(m8rt *rt, uint32_t base)
{
    return (m8rt_raw_read(rt, base + 0x30) & m8rt_raw_read(rt, base + 0x34)
            & m8rt_raw_read(rt, base + 0x38) & (INT_CC | INT_TC | INT_BRR | INT_ERRORS)) != 0;
}
static void irq_check(m8rt *rt, uint32_t base)
{
    int irq = controller_irq(base);
    if (irq >= 0 && irq_live(rt, base)) m8rt_pend(rt, (unsigned)irq);
}
static void irq_line(m8rt *rt, uint32_t base)
{
    int irq = controller_irq(base);
    if (irq < 0) return;
    uint32_t mask = UINT32_C(1) << (irq & 31);
    bool pending = (rt->nvic_ispr[irq >> 5] & mask) != 0;
    if (irq_live(rt, base)) {
        if (!pending) m8rt_pend(rt, (unsigned)irq);
    } else if (pending) m8rt_nvic_pending(rt,irq >> 5,rt->nvic_ispr[irq >> 5] & ~mask);
}
static void set_status(m8rt *rt, uint32_t base, uint32_t bits)
{
    sd_raw_write(rt, base + 0x30, m8rt_raw_read(rt, base + 0x30) | bits);
    irq_check(rt, base);
}
static void write_event(m8rt *rt, uint32_t base, sd_transfer *st,
                        unsigned event, const char *reason)
{
    m8rt_sd *sd = rt->sd;
    sd_event row = {0};
    row.valid = true; row.event = event; row.base = base;
    if (st) {
        row.cmd = st->cmd; row.arg = st->arg; row.write_id = st->write_id;
        row.expected_infinite = st->infinite;
        row.bytes_expected = (uint64_t)st->blksize * st->blkcnt;
        row.bytes_accepted = st->bytes_accepted;
    }
    if (reason) snprintf(row.reason, sizeof(row.reason), "%s", reason);
    switch (event) {
    case 0: sd->started++; sd->pending++; break;
    case 1:
        sd->completed++; if (sd->pending) sd->pending--;
        if (st && st->dma) {
            sd_dma *dma = &controller(rt, base)->dma;
            if (dma->present && dma->write && dma->ok && dma->write_id == st->write_id) {
                dma->completion_valid = true; dma->completion = row;
            }
        }
        break;
    case 2:
        sd->cancelled++; if (sd->pending) sd->pending--;
        if (!sd->first_cancelled.valid) sd->first_cancelled = row;
        break;
    case 3:
        sd->errors++;
        if (!sd->first_error.valid) sd->first_error = row;
        break;
    }
    sd->last = row;
}
static void complete_write(m8rt *rt, uint32_t base, sd_transfer *st, const char *reason)
{
    if (!st->valid || st->history_status != 0) return;
    st->history_status = 1;
    write_event(rt, base, st, 1, reason);
}
static void cancel_write(m8rt *rt, uint32_t base, const char *reason)
{
    sd_transfer *st = &controller(rt, base)->write;
    if (!st->valid || st->history_status != 0) return;
    if (!strcmp(reason, "guest-cmd12")) {
        uint64_t expected = (uint64_t)st->blksize * st->blkcnt;
        uint32_t block_bytes = st->words_per_block * 4;
        if ((!st->infinite && st->bytes_accepted >= expected)
            || (st->infinite && block_bytes && st->bytes_accepted
                && st->bytes_accepted % block_bytes == 0)) {
            complete_write(rt, base, st, reason);
            return;
        }
    }
    st->history_status = 2;
    write_event(rt, base, st, 2, reason);
}

void m8rt_sd_reset(m8rt *rt)
{
    for (unsigned i = 0; i < SD_CONTROLLERS; ++i) {
        cancel_write(rt, SD_LO + (i << 12), "watchdog-reset");
        memset(&rt->sd->controller[i], 0, sizeof(rt->sd->controller[i]));
    }
    /* Accepted bytes, the image, host fault/read-only policy and transaction
     * history survive. A reset never replays or completes a pending write. */
}
static void transfer_start(m8rt *rt, uint32_t base, uint32_t arg, uint32_t cmd,
                           uint32_t xfertyp, bool writing)
{
    m8rt_sd *sd = rt->sd;
    sd_controller *c = controller(rt, base);
    uint32_t blksize = c->block_set && c->blksize ? c->blksize : 512;
    uint32_t blkcnt = c->block_set ? c->blkcnt : writing ? 0 : 1;
    c->guard = false;
    c->fifo_fault = false;
    cancel_write(rt, base, writing ? "superseded-by-write" : "superseded-by-read");
    c->read.valid = false; c->write.valid = false;
    sd_transfer *st = writing ? &c->write : &c->read;
    memset(st, 0, sizeof(*st));
    st->valid = true;
    st->blksize = blksize; st->blkcnt = blkcnt; st->cmd = cmd; st->arg = arg;
    st->words_per_block = blksize / 4 ? blksize / 4 : 1;
    st->words_left = st->words_per_block;
    st->infinite = blkcnt == 0; st->blocks_left = blkcnt;
    st->ptr = ((uint64_t)arg * 512) % sd->image_size; st->start_ptr = st->ptr;
    st->xfertyp = xfertyp;
    st->dma = (m8rt_raw_read(rt, base + 0x48) & MIX_DMAEN) != 0;
    if (writing) {
        /* dma_capture is set only by an actual DMA attempt, below. */
        st->dma = false;
        st->write_id = sd->started + 1;
        write_event(rt, base, st, 0, NULL);
    }
}
static void cmd6_status(uint8_t *payload, uint32_t arg)
{
    memset(payload, 0, 64);
    payload[13] = 3;
    payload[14] = (uint8_t)(((arg >> 20) & 15) << 4 | ((arg >> 16) & 15));
    payload[15] = (uint8_t)(((arg >> 12) & 15) << 4 | ((arg >> 8) & 15));
    payload[16] = (uint8_t)(((arg >> 4) & 15) << 4 | 1);
}
static void dma_error(sd_dma *d, const char *error)
{
    snprintf(d->error, sizeof(d->error), "%s", error);
}
static void complete_cmd6_dma(m8rt *rt, uint32_t base, uint32_t arg)
{
    sd_controller *c = controller(rt, base);
    sd_dma *d = &c->dma;
    memset(d, 0, sizeof(*d));
    d->present = true; d->cmd = 6; d->arg = arg;
    d->dmaen = (m8rt_raw_read(rt, base + 0x48) & MIX_DMAEN) != 0;
    d->dmas = (m8rt_raw_read(rt, base + 0x28) >> 8) & 3;
    uint32_t size = c->block_set && c->blksize ? c->blksize : 64;
    uint32_t count = c->block_set && c->blkcnt ? c->blkcnt : 1;
    d->nbytes = (size_t)size * count;
    if (d->nbytes > 4096) d->nbytes = 4096;
    d->ds_addr = m8rt_raw_read(rt, base);
    uint8_t bytes[4096] = {0};
    if (d->nbytes == 64) {
        cmd6_status(bytes, arg); memcpy(d->payload, bytes, 64); d->switch_status = true;
    }
    if (d->dmaen && !d->dmas && d->ds_addr) {
        int error = m8rt_mem_write(rt, d->ds_addr, bytes, d->nbytes);
        if (!error) d->ok = true;
        else snprintf(d->error, sizeof(d->error), "guest memory write: %d", error);
    } else if (!d->dmaen) dma_error(d, "pio-mode");
    else if (!d->ds_addr) dma_error(d, "null-ds-addr");
    else snprintf(d->error, sizeof(d->error), "dmas=%u", d->dmas);
    if (d->dmaen) {
        set_status(rt, base, INT_TC);
        c->guard = true; c->guard_cmd = 6; c->guard_arg = arg;
    }
}
static void image_write(m8rt_sd *sd, size_t offset, const uint8_t *src, size_t n)
{
    if (n) sd->image_revision++;
    while (n) {
        size_t part = sd->image_size - offset;
        if (part > n) part = n;
        memcpy(sd->image + offset, src, part);
        src += part; n -= part; offset = 0;
    }
}
static const char *write_limit(m8rt_sd *sd, size_t requested, size_t *accepted)
{
    *accepted = requested;
    if (sd->read_only) { *accepted = 0; return "read-only-media"; }
    if (sd->fault_armed) {
        if ((uint64_t)requested >= sd->fault_remaining) {
            *accepted = (size_t)sd->fault_remaining;
            sd->fault_remaining = 0;
            return "injected-write-failure";
        }
        sd->fault_remaining -= requested;
    }
    return NULL;
}
static void fail_write(m8rt *rt, uint32_t base, const char *reason, bool command_error)
{
    sd_controller *c = controller(rt, base);
    /* Preserve the accepted prefix and its history. The failed transfer no
     * longer accepts FIFO bytes, even if the firmware finishes its burst. */
    cancel_write(rt, base, reason);
    write_event(rt, base, &c->write, 3, reason);
    /* TeensySdio checks errors between blocks, but its inner FIFO burst wait
     * has no timeout. Drain ignored words from that burst without dropping
     * BWEN, so it can return to the transfer-status error check. */
    c->fifo_fault = !command_error;
    /* A rejected write does not leave the card programming data. DAT0 must
     * remain high after CMD12, including when no successful DMA is latched. */
    c->error_idle = true;
    c->write.valid = false;
    if (c->dma.present && c->dma.write) {
        c->dma.ok = false;
        dma_error(&c->dma, reason);
    }
    set_status(rt, base, INT_TC | INT_DCE | (command_error ? INT_CCE : 0));
}
static void stage_write_dma(m8rt *rt, uint32_t base, uint32_t cmd, uint32_t arg)
{
    sd_controller *c = controller(rt, base);
    if (!(m8rt_raw_read(rt, base + 0x48) & MIX_DMAEN)) return;
    sd_dma *d = &c->dma;
    memset(d, 0, sizeof(*d));
    d->present = true; d->write = true; d->dmaen = true;
    d->cmd = cmd; d->arg = arg;
    d->dmas = (m8rt_raw_read(rt, base + 0x28) >> 8) & 3;
    uint32_t size = c->block_set && c->blksize ? c->blksize : 512;
    uint32_t count = c->block_set && c->blkcnt ? c->blkcnt : 1;
    d->nbytes = (size_t)size * count;
    if (d->nbytes > (1u << 20)) d->nbytes = 1u << 20;
    d->ds_addr = m8rt_raw_read(rt, base);
    c->write.dma = true; d->write_id = c->write.write_id;
    if (!d->dmas && d->ds_addr) {
        uint8_t *raw = malloc(d->nbytes);
        if (!raw) dma_error(d, "out-of-memory");
        else {
            int error = m8rt_mem_read(rt, d->ds_addr, raw, d->nbytes);
            if (error) snprintf(d->error, sizeof(d->error), "guest memory read: %d", error);
            else {
                size_t offset = ((uint64_t)arg * 512) % rt->sd->image_size;
                size_t accepted;
                const char *failure = write_limit(rt->sd, d->nbytes, &accepted);
                image_write(rt->sd, offset, raw, accepted);
                c->write.bytes_accepted += accepted;
                rt->sd->bytes_accepted += accepted;
                if (failure) {
                    free(raw);
                    fail_write(rt, base, failure, false);
                    return;
                }
                d->ok = true;
                if (!c->write.infinite && c->write.bytes_accepted
                    >= (uint64_t)c->write.blksize * c->write.blkcnt)
                    complete_write(rt, base, &c->write, "dma-transfer-complete");
            }
            free(raw);
        }
    } else if (!d->ds_addr) dma_error(d, "null-ds-addr");
    else snprintf(d->error, sizeof(d->error), "dmas=%u", d->dmas);
    if (d->error[0]) write_event(rt, base, &c->write, 3, d->error);
    set_status(rt, base, INT_TC);
}
static void accept_word(m8rt *rt, uint32_t base, uint32_t value)
{
    sd_controller *c = controller(rt, base);
    sd_transfer *st = &c->write;
    if (!st->valid) {
        /* The DMA ISR clears status and DMAEN. If firmware then continues
         * the same transfer through DATPORT, reject it without replay and
         * make that new data rejection visible to the PIO status check. */
        if (c->fifo_fault) set_status(rt, base, INT_TC | INT_DCE);
        return;
    }
    if (!st->infinite && st->blocks_left <= 0 && !st->words_left) {
        st->valid = false; return;
    }
    uint8_t word[4]; m8rt_st32(word, value);
    size_t accepted;
    const char *failure = write_limit(rt->sd, sizeof(word), &accepted);
    image_write(rt->sd, st->ptr % rt->sd->image_size, word, accepted);
    if (failure) {
        st->ptr = (st->ptr + accepted) % rt->sd->image_size;
        st->bytes_accepted += accepted;
        rt->sd->bytes_accepted += accepted;
        if (accepted) rt->sd->pio_words++;
        fail_write(rt, base, failure, false);
        return;
    }
    st->ptr = (st->ptr + 4) % rt->sd->image_size;
    st->words_left--; st->bytes_accepted += 4;
    rt->sd->bytes_accepted += 4; rt->sd->pio_words++;
    if (!st->words_left) {
        if (st->infinite) st->words_left = st->words_per_block;
        else {
            st->blocks_left--;
            if (st->blocks_left > 0) st->words_left = st->words_per_block;
            else {
                complete_write(rt, base, st, "pio-transfer-complete");
                st->valid = false;
            }
        }
        set_status(rt, base, INT_TC);
    }
}
int m8rt_sd_init(m8rt *rt, const char *path)
{
    m8rt_sd *sd = calloc(1, sizeof(*sd));
    if (!sd) return -1;
    rt->sd = sd;
    sd->private_file = tmpfile();
    if (!sd->private_file) { m8rt_sd_destroy(rt); return -1; }
    size_t n = 4u * 1024 * 1024;
    FILE *source = NULL;
    if (path) {
        source = fopen(path, "rb");
        if (!source || fseek(source, 0, SEEK_END) || ftell(source) <= 0) {
            if (source) fclose(source);
            m8rt_sd_destroy(rt); return -1;
        }
        long length = ftell(source);
        if ((uintmax_t)length > SIZE_MAX || fseek(source, 0, SEEK_SET)) {
            fclose(source); m8rt_sd_destroy(rt); return -1;
        }
        n = (size_t)length;
    }
    sd->image = calloc(1, n); sd->image_size = n;
    if (!sd->image || (source && fread(sd->image, 1, n, source) != n)) {
        if (source) fclose(source);
        m8rt_sd_destroy(rt); return -1;
    }
    if (source) fclose(source);
    return 0;
}
void m8rt_sd_destroy(m8rt *rt)
{
    if (!rt->sd) return;
    free(rt->sd->image);
    if (rt->sd->private_file) fclose(rt->sd->private_file);
    free(rt->sd); rt->sd = NULL;
}
bool m8rt_sd_handles(uint32_t addr) { return addr >= SD_LO && addr < SD_HI; }
uint32_t m8rt_sd_read(m8rt *rt, uint32_t addr)
{
    uint32_t a = addr & ~UINT32_C(3), base = a & ~UINT32_C(0xfff), off = a & 0xfff;
    sd_controller *c = controller(rt, base);
    uint32_t value = m8rt_raw_read(rt, a);
    switch (off) {
    case 0x2c: return 1;
    case 0x24: {
        uint32_t out = (1u << 16) | (1u << 8) | (1u << 3);
        bool read_pending = transfer_pending(&c->read), latched = false;
        if (read_pending) out |= (1u << 11) | (1u << 9);
        if (transfer_pending(&c->write)) out |= (1u << 10) | (1u << 8);
        if (c->fifo_fault) out |= 1u << 10;
        for (unsigned i = 0; i < SD_CONTROLLERS; i++)
            if (rt->sd->controller[i].dma.present && rt->sd->controller[i].dma.ok) latched = true;
        if (read_pending || c->guard || latched || c->error_idle) out |= 1u << 24;
        return out;
    }
    case 0x10: case 0x14: case 0x18: case 0x1c:
    case 0x30: case 0x34: case 0x38: return value;
    case 0x20: {
        sd_transfer *st = &c->read;
        if (!transfer_pending(st)) return 0;
        uint32_t word;
        {
            uint8_t bytes[4] = {0};
            size_t ptr = st->ptr % rt->sd->image_size;
            size_t count = rt->sd->image_size - ptr;
            if (count > 4) count = 4;
            memcpy(bytes, rt->sd->image + ptr, count);
            word = m8rt_le32(bytes);
            st->ptr = (st->ptr + 4) % rt->sd->image_size;
        }
        st->words_left--; st->words_consumed++; rt->sd->pio_words++;
#ifdef M8RT_ACCESS_PROFILE
        if(!st->cmd6 && (st->cmd==17 || st->cmd==18)) m8rt_access_storage_read(rt,4);
#endif
        if (!st->words_left) {
            if (st->infinite) {
                st->words_left = st->words_per_block;
                set_status(rt, base, INT_BRR | INT_TC);
            } else {
                st->blocks_left--;
                if (st->blocks_left > 0) {
                    st->words_left = st->words_per_block;
                    set_status(rt, base, INT_BRR);
                } else {
                    c->guard = false;
                    set_status(rt, base, INT_TC);
                }
            }
        }
        return word;
    }
    default: return value | 1;
    }
}
void m8rt_sd_write(m8rt *rt, uint32_t addr, uint32_t value)
{
    uint32_t a = addr & ~UINT32_C(3), base = a & ~UINT32_C(0xfff), off = a & 0xfff;
    sd_controller *c = controller(rt, base);
    if (off != 0x20) {
        if (off == 0x2c && (value & (1u << 26))) {
            cancel_write(rt, base, "guest-rstd"); c->fifo_fault = false;
        }
        if (rt->sd->trace_len < TRACE_CAP) {
            rt->sd->trace[rt->sd->trace_len][0] = a;
            rt->sd->trace[rt->sd->trace_len++][1] = value;
        }
    }
    if (off == 0x04) {
        c->block_set = true; c->blksize = value & 0xfff; c->blkcnt = (value >> 16) & 0x1fff;
    }
    if (off == 0x0c) {
        uint32_t cmd = (value >> 24) & 0x3f, arg = m8rt_raw_read(rt, base + 8);
        bool duplicate = c->guard && c->guard_cmd == cmd && c->guard_arg == arg;
        bool data = (value & (1u << 21)) != 0;
        if (c->guard && !duplicate) c->guard = false;
        switch (cmd) {
        case 8: sd_raw_write(rt, base + 0x10, 0x1aa); break;
        case 55:
            sd_raw_write(rt, base + 0x10, 0x900); c->app = true; break;
        case 41:
            if (c->app) { c->app = false; sd_raw_write(rt, base + 0x10, 0xc0ff8000); }
            break;
        case 2:
            sd_raw_write(rt, base + 0x10, 0x03534453);
            sd_raw_write(rt, base + 0x14, 0x44423030);
            sd_raw_write(rt, base + 0x18, 0x30445680);
            sd_raw_write(rt, base + 0x1c, 0x0955012a); break;
        case 3: sd_raw_write(rt, base + 0x10, 0x45670000); break;
        case 7: case 13: case 16: sd_raw_write(rt, base + 0x10, 0x900); break;
        case 9:
            sd_raw_write(rt, base + 0x10, 0x400e0032);
            sd_raw_write(rt, base + 0x14, 0x5b590000);
            sd_raw_write(rt, base + 0x18, 0x3fff7f80);
            sd_raw_write(rt, base + 0x1c, 0x0a400000); break;
        case 17: case 18:
            sd_raw_write(rt, base + 0x10, 0x900);
            transfer_start(rt, base, arg, cmd, value, false);
            set_status(rt, base, INT_BRR); break;
        case 6:
            sd_raw_write(rt, base + 0x10, 0x900);
            if (data) {
                if (duplicate) {
                    sd_raw_write(rt, base + 0x30, m8rt_raw_read(rt, base + 0x30)
                                    | INT_BRR | INT_TC | INT_CC);
                    c->guard = false;
                } else {
                    memset(&c->read, 0, sizeof(c->read));
                    c->read.valid = true; c->read.cmd6 = true; c->read.blocks_left = 1;
                    c->read.words_left = c->read.words_per_block = 16;
                    set_status(rt, base, INT_BRR);
                    complete_cmd6_dma(rt, base, arg);
                }
            }
            break;
        case 24: case 25:
            sd_raw_write(rt, base + 0x10, 0x900);
            if (data) {
                transfer_start(rt, base, arg, cmd, value, true);
                if (rt->sd->read_only) {
                    /* R1 WP_VIOLATION plus a controller command error makes
                     * both card-status and command-status checks fail. */
                    sd_raw_write(rt, base + 0x10, 0x900 | (1u << 26));
                    fail_write(rt, base, "read-only-media", true);
                    break;
                }
                if (rt->sd->fault_armed && !rt->sd->fault_remaining) {
                    fail_write(rt, base, "injected-write-failure", true);
                    break;
                }
                stage_write_dma(rt, base, cmd, arg);
            }
            break;
        case 12:
            sd_raw_write(rt, base + 0x10, 0x900);
            c->read.valid = false;
            c->fifo_fault = false;
            cancel_write(rt, base, "guest-cmd12"); c->write.valid = false;
            set_status(rt, base, INT_TC); break;
        default: break;
        }
        if (!(cmd == 6 && data && duplicate)) set_status(rt, base, INT_CC);
    }
    if (off == 0x20) accept_word(rt, base, value);
    if (off == 0x30) {
        sd_raw_write(rt, a, m8rt_raw_read(rt, a) & ~value); irq_line(rt, base); return;
    }
    sd_raw_write(rt, a, value);
    if (off == 0x34 || off == 0x38) irq_line(rt, base);
}
int m8rt_sd_replace_image(m8rt *rt, const void *data, size_t n)
{
    if (!rt || !rt->sd || rt->in_emu || !data || !n) return -1;
    uint8_t *replacement = malloc(n);
    if (!replacement) return -1;
    memcpy(replacement, data, n);
    free(rt->sd->image); rt->sd->image = replacement; rt->sd->image_size = n;
    rt->sd->image_revision++;
    return 0;
}
uint64_t m8rt_sd_image_revision(m8rt *rt) { return rt && rt->sd ? rt->sd->image_revision : 0; }
size_t m8rt_sd_image_size(m8rt *rt) { return rt && rt->sd ? rt->sd->image_size : 0; }
int m8rt_sd_copy_image(m8rt *rt, void *out, size_t n)
{
    if (!rt || !rt->sd || rt->in_emu || !out || n != rt->sd->image_size) return -1;
    memcpy(out, rt->sd->image, n); return 0;
}
int m8rt_sd_read_image(m8rt *rt, size_t offset, void *out, size_t n)
{
    if (!rt || !rt->sd || rt->in_emu || !out
        || offset > rt->sd->image_size || n > rt->sd->image_size - offset) return -1;
    memcpy(out, rt->sd->image + offset, n); return 0;
}
int m8rt_sd_set_read_only(m8rt *rt, int enabled)
{
    if (!rt || !rt->sd || rt->in_emu) return -1;
    rt->sd->read_only = enabled != 0;
    return 0;
}
int m8rt_sd_set_write_fault(m8rt *rt, int enabled, uint64_t after_bytes)
{
    if (!rt || !rt->sd || rt->in_emu) return -1;
    rt->sd->fault_armed = enabled != 0;
    rt->sd->fault_remaining = after_bytes;
    return 0;
}
int m8rt_sd_flush(m8rt *rt)
{
    if (!rt || !rt->sd || rt->in_emu) return -1;
    m8rt_sd *sd = rt->sd;
    if (fseek(sd->private_file, 0, SEEK_SET)
        || fwrite(sd->image, 1, sd->image_size, sd->private_file) != sd->image_size
        || fflush(sd->private_file)
        || ftruncate(fileno(sd->private_file), (off_t)sd->image_size)
        || fsync(fileno(sd->private_file))) return -1;
    return 0;
}
bool m8rt_sd_active(m8rt *rt)
{
    if (!rt || !rt->sd) return false;
    for (unsigned i = 0; i < SD_CONTROLLERS; i++) {
        sd_controller *c = &rt->sd->controller[i];
        if (transfer_pending(&c->read) || transfer_pending(&c->write) || c->guard) return true;
    }
    return false;
}
int m8rt_sd_abort_read(m8rt *rt)
{
    if (!rt || !rt->sd || rt->in_emu) return -1;
    for (unsigned i = 0; i < SD_CONTROLLERS; i++) {
        sd_controller *c = &rt->sd->controller[i];
        if (transfer_pending(&c->write) || c->guard) return -1;
        sd_transfer *r = &c->read;
        if (transfer_pending(r) && (i != 0 || r->cmd6 || r->cmd != 18 || !r->infinite
                                   || r->blksize != 512 || r->words_per_block != 128)) return -1;
    }
    int count = 0;
    for (unsigned i = 0; i < SD_CONTROLLERS; i++) {
        sd_transfer *r = &rt->sd->controller[i].read;
        if (transfer_pending(r)) { r->valid = false; count++; }
    }
    return count;
}

static int export_refused(m8rt *rt, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(rt->error, sizeof(rt->error), format, args);
    va_end(args);
    return -1;
}
int m8rt_sd_prepare_export(m8rt *rt, int abort_read)
{
    if (!rt) return -1;
    if (!rt->sd) return export_refused(rt, "SD export refused: storage is absent");
    if (rt->in_emu) return export_refused(rt, "SD export refused: execution is active");
    if (rt->fatal) return -1; /* Keep the original runtime error. */
    m8rt_sd *sd = rt->sd;
    if (sd->errors) return export_refused(rt,
        "SD export refused: write errors=%" PRIu64 ", first=%s, accepted=%" PRIu64,
        sd->errors, sd->first_error.reason, sd->bytes_accepted);
    if (sd->read_only) return export_refused(rt, "SD export refused: media is read-only");
    if (sd->pending || sd->cancelled || sd->started != sd->completed)
        return export_refused(rt, "SD export refused: unresolved write history: "
            "pending=%" PRIu64 ", cancelled=%" PRIu64 ", started=%" PRIu64 ", completed=%" PRIu64,
            sd->pending, sd->cancelled, sd->started, sd->completed);
    if (!sd->image_size || sd->image_size % 512)
        return export_refused(rt, "SD export refused: invalid image bytes=%zu", sd->image_size);
    for (unsigned i = 0; i < SD_CONTROLLERS; i++) {
        sd_controller *c = &sd->controller[i];
        uint32_t base = SD_LO + (i << 12);
        if (transfer_pending(&c->write) || c->guard)
            return export_refused(rt, "SD export refused: controller=%08x, write_pending=%u, guard=%u",
                base, transfer_pending(&c->write), c->guard);
        sd_dma *d = &c->dma;
        if (d->present && (!d->ok || d->error[0]
            || (d->write && (!d->completion_valid || d->completion.event != 1
                || d->completion.write_id != d->write_id || d->completion.bytes_accepted < d->nbytes))
            || (!d->write && d->cmd != 6)))
            return export_refused(rt, "SD export refused: controller=%08x DMA: "
                "ok=%u, error=%s, write=%u, cmd=%u, completion_valid=%u, bytes=%zu",
                base, d->ok, d->error, d->write, d->cmd, d->completion_valid, d->nbytes);
        sd_transfer *r = &c->read;
        if (!transfer_pending(r)) continue;
        if (!abort_read) return export_refused(rt,
            "SD export refused: controller=%08x read is active, cmd=%u", base, r->cmd);
        if (i != 0 || r->cmd6 || r->cmd != 18 || !r->infinite || r->blkcnt
            || r->blksize != 512 || r->words_per_block != 128
            || !r->words_left || r->words_left > 128)
            return export_refused(rt, "SD export refused: unsupported terminal read: "
                "controller=%08x, cmd=%u, cmd6=%u, infinite=%u, blkcnt=%u, blksize=%u, words=%u/%u",
                base, r->cmd, r->cmd6, r->infinite, r->blkcnt, r->blksize,
                r->words_left, r->words_per_block);
        if (c->blksize != 512 || c->blkcnt)
            return export_refused(rt, "SD export refused: block configuration: blksize=%u, blkcnt=%u",
                c->blksize, c->blkcnt);
        uint32_t xfer = m8rt_raw_read(rt, base + 0x0c);
        uint32_t mix = m8rt_raw_read(rt, base + 0x48);
        uint32_t prot = m8rt_raw_read(rt, base + 0x28);
        uint32_t reset = m8rt_raw_read(rt, base + 0x2c);
        uint32_t arg = m8rt_raw_read(rt, base + 0x08);
        uint32_t blocks = m8rt_raw_read(rt, base + 0x04);
        if (xfer != r->xfertyp || ((xfer >> 24) & 63) != 18
            || !(xfer & (1u << 21)) || (xfer & 1)
            || (r->dma ? (xfer != 0x123a0000 || !(mix & MIX_DMAEN) || ((prot >> 8) & 3))
                       : (mix & MIX_DMAEN))
            || (reset & (1u << 26)) || arg != r->arg || blocks != 512)
            return export_refused(rt, "SD export refused: read registers: xfer=%08x/%08x, "
                "dma=%u, mix=%08x, prot=%08x, reset=%08x, arg=%u/%u, blocks=%08x",
                xfer, r->xfertyp, r->dma, mix, prot, reset, arg, r->arg, blocks);
        size_t start = ((uint64_t)r->arg * 512) % sd->image_size;
        size_t progressed = (size_t)(r->words_consumed % (sd->image_size / 4)) * 4;
        if (r->ptr >= sd->image_size || (r->ptr & 3) || r->start_ptr != start
            || r->ptr != (start + progressed) % sd->image_size
            || r->words_left != 128 - r->words_consumed % 128)
            return export_refused(rt, "SD export refused: read cursor: ptr=%zu, start=%zu/%zu, "
                "words_consumed=%" PRIu64 ", words_left=%u, image_bytes=%zu",
                r->ptr, r->start_ptr, start, r->words_consumed, r->words_left, sd->image_size);
    }
    /* All admission checks precede the only partial effect. */
    if (abort_read && m8rt_sd_abort_read(rt) < 0)
        return export_refused(rt, "SD export refused: terminal read retirement failed");
    return 0;
}

typedef struct { char *out; size_t cap, used; } json_writer;
static void json_put(json_writer *j, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    size_t room = j->used < j->cap ? j->cap - j->used : 0;
    int length = vsnprintf(room ? j->out + j->used : NULL, room, format, args);
    va_end(args);
    if (length > 0) j->used += (size_t)length;
}
static void json_string(json_writer *j, const char *value)
{
    json_put(j, "\"");
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (*p == '"' || *p == '\\') json_put(j, "\\%c", *p);
        else if (*p < 32) json_put(j, "\\u%04x", *p);
        else json_put(j, "%c", *p);
    }
    json_put(j, "\"");
}
static void json_blocks(json_writer *j, const sd_transfer *st)
{
    if (st->infinite) json_put(j, "null");
    else json_put(j, "%" PRId64, st->blocks_left);
}
static void json_transfer(json_writer *j, const sd_transfer *st, bool writing)
{
    json_put(j, "{\"ptr\":%zu,\"blocks_left\":", st->ptr);
    json_blocks(j, st);
    json_put(j, ",\"words_left\":%u,\"words_per_block\":%u", st->words_left, st->words_per_block);
    if (st->cmd6 && st->words_consumed)
        json_put(j, ",\"words_consumed\":%" PRIu64, st->words_consumed);
    if (!st->cmd6) {
        json_put(j, ",\"cmd\":%u,\"arg\":%u,\"start_lba\":%u,\"blkcnt\":%u,\"blksize\":%u",
                 st->cmd, st->arg, st->arg, st->blkcnt, st->blksize);
        if (writing) {
            json_put(j, ",\"bytes_expected\":");
            if (st->infinite) json_put(j, "null");
            else json_put(j, "%" PRIu64, (uint64_t)st->blksize * st->blkcnt);
            json_put(j, ",\"bytes_accepted\":%" PRIu64 ",\"history_status\":\"%s\",\"write_id\":%" PRIu64,
                     st->bytes_accepted, st->history_status == 0 ? "pending"
                     : st->history_status == 1 ? "completed" : "cancelled", st->write_id);
            if (st->dma) json_put(j, ",\"dma_capture\":true");
        } else {
            json_put(j, ",\"start_ptr\":%zu,\"words_consumed\":%" PRIu64 ",\"mode\":\"%s\",\"xfertype\":%u",
                     st->start_ptr, st->words_consumed, st->dma ? "dma" : "pio", st->xfertyp);
        }
    }
    json_put(j, "}");
}
static void json_event(json_writer *j, const sd_event *event, bool generation)
{
    if (!event->valid) { json_put(j, "null"); return; }
    static const char *names[] = {"started", "completed", "cancelled", "error"};
    json_put(j, "{\"event\":\"%s\",\"base\":%u,\"write_id\":%" PRIu64
             ",\"cmd\":%u,\"arg\":%u,\"bytes_expected\":", names[event->event],
             event->base, event->write_id, event->cmd, event->arg);
    if (event->expected_infinite) json_put(j, "null");
    else json_put(j, "%" PRIu64, event->bytes_expected);
    json_put(j, ",\"bytes_accepted\":%" PRIu64, event->bytes_accepted);
    if (event->reason[0]) {
        json_put(j, ",\"%s\":", event->event == 3 ? "error" : "reason");
        json_string(j, event->reason);
    }
    if (generation) json_put(j, ",\"generation\":\"\"");
    json_put(j, "}");
}
static void json_dma(json_writer *j, const sd_dma *d)
{
    json_put(j, "{\"ds_addr\":\"0x%x\",\"nbytes\":%zu,\"dmaen\":%s,\"dmas\":%u,\"cmd\":%u,\"arg\":\"0x%x\",\"ok\":%s,\"err\":",
             d->ds_addr, d->nbytes, d->dmaen ? "true" : "false", d->dmas,
             d->cmd, d->arg, d->ok ? "true" : "false");
    if (d->error[0]) json_string(j, d->error); else json_put(j, "null");
    if (d->write) {
        json_put(j, ",\"dir\":\"write\",\"lba\":\"0x%x\",\"write_id\":%" PRIu64
                 ",\"storage_generation\":\"\",\"completion\":", d->arg, d->write_id);
        json_event(j, &d->completion, true);
    } else {
        json_put(j, ",\"switch_status\":");
        if (d->switch_status) {
            json_put(j, "\"");
            for (unsigned i = 0; i < sizeof(d->payload); i++) json_put(j, "%02x", d->payload[i]);
            json_put(j, "\"");
        } else json_put(j, "null");
        if (d->dmaen) json_put(j, ",\"tc_guarded\":true");
    }
    json_put(j, "}");
}
size_t m8rt_sd_snapshot_json(m8rt *rt, char *out, size_t n)
{
    json_writer j = {out, out ? n : 0, 0};
    if (!rt || !rt->sd) { json_put(&j, "{}"); return j.used + 1; }
    m8rt_sd *sd = rt->sd;
    const char *inventory[] = {"_sd_read", "_sd_write", "_sd_tc_open", "_sd_sdma", "_sd_blk"};
    json_put(&j, "{");
    for (unsigned kind = 0; kind < 5; kind++) {
        bool comma = false;
        json_put(&j, "%s\"%s\":{", kind ? "," : "", inventory[kind]);
        for (unsigned i = 0; i < SD_CONTROLLERS; i++) {
            sd_controller *c = &sd->controller[i];
            bool valid = kind == 0 ? c->read.valid : kind == 1 ? c->write.valid
                : kind == 2 ? c->guard : kind == 3 ? c->dma.present : c->block_set;
            if (!valid) continue;
            json_put(&j, "%s\"%u\":", comma ? "," : "", SD_LO + (i << 12)); comma = true;
            switch (kind) {
            case 0: json_transfer(&j, &c->read, false); break;
            case 1: json_transfer(&j, &c->write, true); break;
            case 2: json_put(&j, "{\"cmd\":%u,\"arg\":%u}", c->guard_cmd, c->guard_arg); break;
            case 3: json_dma(&j, &c->dma); break;
            case 4: json_put(&j, "{\"blksize\":%u,\"blkcnt\":%u}", c->blksize, c->blkcnt); break;
            }
        }
        json_put(&j, "}");
    }
    json_put(&j, ",\"_sd_write_safety\":{\"started\":%" PRIu64 ",\"completed\":%" PRIu64
             ",\"cancelled\":%" PRIu64 ",\"errors\":%" PRIu64 ",\"pending\":%" PRIu64
             ",\"bytes_accepted\":%" PRIu64 ",\"first_cancelled\":", sd->started, sd->completed,
             sd->cancelled, sd->errors, sd->pending, sd->bytes_accepted);
    json_event(&j, &sd->first_cancelled, false);
    json_put(&j, ",\"first_error\":"); json_event(&j, &sd->first_error, false);
    json_put(&j, ",\"last\":"); json_event(&j, &sd->last, false);
    json_put(&j, "},\"_sd_write_policy\":{\"read_only\":%s,\"fault_armed\":%s,"
             "\"fault_remaining\":%" PRIu64 "},\"sd_pio_words\":%" PRIu64
             ",\"image_revision\":%" PRIu64 ",\"usdhc_trace\":[",
             sd->read_only ? "true" : "false", sd->fault_armed ? "true" : "false",
             sd->fault_remaining, sd->pio_words, sd->image_revision);
    for (size_t i = 0; i < sd->trace_len; i++)
        json_put(&j, "%s[%u,%u]", i ? "," : "", sd->trace[i][0], sd->trace[i][1]);
    json_put(&j, "],\"mem\":{");
    bool comma = false;
    for (unsigned i = 0; i < SD_CONTROLLERS; i++) {
        sd_controller *c = &sd->controller[i];
        for (unsigned word = 0; word < 1024; word++) {
            if (!c->present[word]) continue;
            uint32_t addr = SD_LO + (i << 12) + (word << 2);
            json_put(&j, "%s\"%u\":%u", comma ? "," : "", addr, m8rt_raw_read(rt, addr)); comma = true;
        }
    }
    json_put(&j, "}}");
    return j.used + 1;
}

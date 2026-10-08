#include "internal.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Teensy 4.1 EEPROM uses 63 append-only 4 KiB sectors. The final flash
 * page is board identification; neither it nor program bytes are writable.
 * Source contract: PJRC teensy4/eeprom.c, eepromemu_flash_write/flash_wait. */
#define FLEX UINT32_C(0x402a8000)
#define NOR_OFFSET UINT32_C(0x7c0000)
#define NOR_BYTES (63u * 4096u)
#define DONE UINT32_C(1)
#define TXWE UINT32_C(0x40)

struct m8rt_nor {
    char *path;
    bool dirty, write_enable, pending, flush_failed;
    uint32_t address, length, written;
    uint8_t tx[8];
    uint64_t commands, programs, programmed_bytes, erases, erased_bytes, flushes;
};

static uint8_t *bytes(m8rt *rt)
{
    return rt->memory.flash.host + NOR_OFFSET;
}

int m8rt_nor_init(m8rt *rt)
{
    rt->nor = calloc(1, sizeof(*rt->nor));
    return rt->nor ? 0 : -1;
}

void m8rt_nor_destroy(m8rt *rt)
{
    if (!rt->nor) return;
    free(rt->nor->path);
    free(rt->nor);
    rt->nor = NULL;
}

void m8rt_nor_reset(m8rt *rt)
{
    if (!rt || !rt->nor) return;
    rt->nor->write_enable = rt->nor->pending = false;
    rt->nor->address = rt->nor->length = rt->nor->written = 0;
    memset(rt->nor->tx, 0, sizeof(rt->nor->tx));
}

int m8rt_nor_set_file(m8rt *rt, const char *path)
{
    if (!rt || !rt->nor || rt->started || rt->in_emu || rt->fatal) return -1;
    if (path && !*path) { m8rt_fail(rt, "NOR settings path is empty"); return -1; }
    m8rt_nor *n = rt->nor;
    char *copy = path ? strdup(path) : NULL;
    if (path && !copy) { m8rt_fail(rt, "NOR settings path allocation failed"); return -1; }
    if (path) {
        FILE *f = fopen(path, "rb");
        if (!f && errno != ENOENT) {
            m8rt_fail(rt, "NOR settings open failed path=%s errno=%d", path, errno);
            free(copy); return -1;
        }
        if (f) {
            uint8_t *input = malloc(NOR_BYTES);
            if (!input) { fclose(f); free(copy); m8rt_fail(rt, "NOR input allocation failed"); return -1; }
            size_t size = fread(input, 1, NOR_BYTES, f);
            int extra = fgetc(f), error = ferror(f);
            fclose(f);
            if (size != NOR_BYTES || extra != EOF || error) {
                m8rt_fail(rt, "NOR settings size/read failed path=%s bytes=%zu expected=%u extra=%d error=%d",
                          path, size, NOR_BYTES, extra, error);
                free(input); free(copy); return -1;
            }
            memcpy(bytes(rt), input, NOR_BYTES);
            free(input);
        }
    }
    free(n->path); n->path = copy;
    return 0;
}

int m8rt_nor_set_output(m8rt *rt, const char *path)
{
    if (!rt || !rt->nor || rt->started || rt->in_emu || rt->fatal) return -1;
    if (path && !*path) { m8rt_fail(rt, "NOR output path is empty"); return -1; }
    char *copy = path ? strdup(path) : NULL;
    if (path && !copy) { m8rt_fail(rt, "NOR output path allocation failed"); return -1; }
    free(rt->nor->path); rt->nor->path = copy;
    rt->nor->dirty = path != NULL;
    return 0;
}

int m8rt_nor_flush(m8rt *rt)
{
    if (!rt || !rt->nor || rt->in_emu) return -1;
    m8rt_nor *n = rt->nor;
    if (n->flush_failed) return -1;
    if (!n->dirty || !n->path) return 0;
    char *temporary = malloc(strlen(n->path) + sizeof(".tmp.XXXXXX"));
    char *directory = strdup(n->path);
    if (!temporary || !directory) {
        free(temporary); free(directory);
        n->flush_failed = true; m8rt_fail(rt, "NOR flush allocation failed"); return -1;
    }
    strcpy(temporary, n->path); strcat(temporary, ".tmp.XXXXXX");
    char *slash = strrchr(directory, '/');
    if (slash) { if (slash == directory) slash[1] = 0; else *slash = 0; }
    else strcpy(directory, ".");
    int fd = mkstemp(temporary), dirfd = -1, error = 0;
    const char *stage = "temporary open";
    if (fd < 0) { error = errno; goto failure; }
    size_t written = 0;
    stage = "write";
    while (written < NOR_BYTES) {
        ssize_t count = write(fd, bytes(rt) + written, NOR_BYTES - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { error = count < 0 ? errno : EIO; goto failure; }
        written += (size_t)count;
    }
    stage = "file fsync";
    if (fsync(fd)) { error = errno; goto failure; }
    stage = "file close";
    if (close(fd)) { fd = -1; error = errno; goto failure; }
    fd = -1;
    stage = "directory open";
    dirfd = open(directory, O_RDONLY);
    if (dirfd < 0) { error = errno; goto failure; }
    stage = "rename";
    if (rename(temporary, n->path)) { error = errno; goto failure; }
    stage = "directory fsync";
    if (fsync(dirfd)) { error = errno; goto failure; }
    close(dirfd); free(temporary); free(directory);
    n->dirty = false;
    ++n->flushes;
    return 0;
failure:
    if (fd >= 0) close(fd);
    if (dirfd >= 0) close(dirfd);
    unlink(temporary);
    free(temporary); free(directory);
    n->flush_failed = true;
    m8rt_fail(rt, "NOR atomic flush failed stage=%s path=%s errno=%d", stage, n->path, error);
    return -1;
}

static bool valid_range(m8rt *rt, unsigned opcode, uint32_t address, uint32_t length)
{
    const char *predicate = NULL;
    if (address < NOR_OFFSET) predicate = "address>=EEPROM_base";
    else if (length > NOR_BYTES) predicate = "length<=EEPROM_size";
    else if (address - NOR_OFFSET > NOR_BYTES - length) predicate = "end<=EEPROM_end";
    if (predicate) {
        m8rt_fail(rt, "NOR command outside EEPROM range first_false=%s opcode=%02x address=%08x length=%u base=%08x size=%u",
                  predicate, opcode, address, length, NOR_OFFSET, NOR_BYTES);
        return false;
    }
    return true;
}

static void trigger(m8rt *rt)
{
    m8rt_nor *n = rt->nor;
    uint32_t control = m8rt_raw_read(rt, FLEX + 0xa4);
    unsigned sequence = (control >> 16) & 15;
    uint32_t lut = FLEX + 0x200 + sequence * 16;
    uint32_t first = m8rt_raw_read(rt, lut), second = m8rt_raw_read(rt, lut + 4);
    unsigned opcode = first & 255;
    uint32_t address = m8rt_raw_read(rt, FLEX + 0xa0), length = control & 65535;
    ++n->commands;
    if (n->pending) {
        m8rt_fail(rt, "NOR command while program pending opcode=%02x address=%08x length=%u written=%u pending_length=%u",
                  opcode, address, length, n->written, n->length); return;
    }
    const char *predicate = NULL;
    if ((first & 0xff00) != 0x0400) predicate = "CMD_SDR_one_pad";
    else if (control & 0x07000000) predicate = "ISEQNUM==0";
    if (predicate) {
        m8rt_fail(rt, "NOR unsupported LUT command first_false=%s opcode=%02x IPCR1=%08x LUT0=%08x LUT1=%08x",
                  predicate, opcode, control, first, second); return;
    }
    m8rt_raw_write(rt, FLEX + 0x14, m8rt_raw_read(rt, FLEX + 0x14) & ~(DONE | TXWE));
    uint32_t expected0 = opcode == 0x06 ? 0x0406 : opcode == 0x05 ? 0x24010405 :
                         UINT32_C(0x08180400) | opcode;
    uint32_t expected1 = opcode == 0x32 ? 0x2201 : 0;
    if (opcode != 0x06 && opcode != 0x05 && opcode != 0x32 &&
        opcode != 0x20 && opcode != 0x52 && opcode != 0xd8) predicate = "opcode_supported";
    else if (first != expected0) predicate = "LUT0==expected";
    else if (second != expected1) predicate = "LUT1==expected";
    else if (opcode == 0x05 && length != 1) predicate = "status_length==1";
    else if (opcode != 0x05 && opcode != 0x32 && length) predicate = "command_length==0";
    if (predicate) {
        m8rt_fail(rt, "NOR unsupported opcode/LUT first_false=%s opcode=%02x address=%08x length=%u LUT0=%08x LUT1=%08x expected0=%08x expected1=%08x",
                  predicate, opcode, address, length, first, second, expected0, expected1); return;
    }
    if (opcode == 0x06) {
        n->write_enable = true;
    } else if (opcode == 0x05) {
        m8rt_raw_write(rt, FLEX + 0x100, n->write_enable ? 2 : 0);
    } else if (opcode == 0x32) {
        if (!length) predicate = "length>0";
        else if (length > 256) predicate = "length<=256";
        else if ((address & 255) + length > 256) predicate = "program_within_page";
        if (predicate) {
            m8rt_fail(rt, "NOR unsupported page program first_false=%s address=%08x length=%u page_offset=%u",
                      predicate, address, length, address & 255); return;
        }
        if (!valid_range(rt, opcode, address, length)) return;
        if (n->write_enable) {
            n->address = address; n->length = length; n->written = 0; n->pending = true;
            ++n->programs;
            m8rt_raw_write(rt, FLEX + 0x14, m8rt_raw_read(rt, FLEX + 0x14) | TXWE);
            return;
        }
    } else {
        uint32_t size = opcode == 0x20 ? 4096 : opcode == 0x52 ? 32768 : 65536;
        address &= ~(size - 1);
        if (!valid_range(rt, opcode, address, size)) return;
        if (n->write_enable) {
            memset(rt->memory.flash.host + address, 255, size);
            n->dirty = true; n->write_enable = false;
            ++n->erases; n->erased_bytes += size;
        }
    }
    m8rt_raw_write(rt, FLEX + 0x14, m8rt_raw_read(rt, FLEX + 0x14) | DONE);
}

bool m8rt_nor_read(m8rt *rt, uint32_t address, uint32_t *value)
{
    if (!rt->nor || address < FLEX || address >= FLEX + 0x400) return false;
    uint32_t a = address & ~UINT32_C(3);
    *value = m8rt_raw_read(rt, a);
    if (a == FLEX) *value &= ~UINT32_C(1);
    return true;
}

bool m8rt_nor_write(m8rt *rt, uint32_t address, uint32_t value)
{
    if (!rt->nor || address < FLEX || address >= FLEX + 0x400) return false;
    m8rt_nor *n = rt->nor;
    uint32_t a = address & ~UINT32_C(3);
    if (a == FLEX + 0x14) {
        m8rt_raw_write(rt, a, m8rt_raw_read(rt, a) & ~value);
        if ((value & TXWE) && n->pending) {
            unsigned count = n->length - n->written;
            if (count > 8) count = 8;
            for (unsigned i = 0; i < count; ++i)
                rt->memory.flash.host[n->address + n->written + i] &= n->tx[i];
            n->dirty = true; n->written += count;
            n->programmed_bytes += count;
            memset(n->tx, 0, sizeof(n->tx));
            if (n->written == n->length) {
                n->pending = false; n->write_enable = false;
                m8rt_raw_write(rt, a, m8rt_raw_read(rt, a) | DONE);
            } else m8rt_raw_write(rt, a, m8rt_raw_read(rt, a) | TXWE);
        }
    } else if (a == FLEX + 0xb0) {
        m8rt_raw_write(rt, a, value);
        if (value & 1) trigger(rt);
    } else if (a == FLEX + 0xbc) {
        if (value & 1) memset(n->tx, 0, sizeof(n->tx));
        m8rt_raw_write(rt, a, value & ~UINT32_C(1));
    } else if (a == FLEX + 0xb8) {
        if (value & 1) m8rt_raw_write(rt, FLEX + 0x100, 0);
        m8rt_raw_write(rt, a, value & ~UINT32_C(1));
    } else if (address >= FLEX + 0x180 && address < FLEX + 0x188) {
        unsigned offset = address - (FLEX + 0x180);
        for (unsigned i = 0; i < 4 && offset + i < 8; ++i) n->tx[offset + i] = (uint8_t)(value >> (8 * i));
        m8rt_raw_write(rt, a, value);
    } else m8rt_raw_write(rt, a, a == FLEX ? value & ~UINT32_C(1) : value);
    return true;
}

uint64_t m8rt_nor_get(m8rt *rt, unsigned field)
{
    if (!rt || !rt->nor) return 0;
    m8rt_nor *n = rt->nor;
    switch (field) {
    case 0: return n->commands;
    case 1: return n->programs;
    case 2: return n->programmed_bytes;
    case 3: return n->erases;
    case 4: return n->erased_bytes;
    case 5: return n->flushes;
    case 6: return n->address;
    case 7: return n->length;
    case 8: return n->pending;
    case 9: return n->dirty;
    default: return 0;
    }
}

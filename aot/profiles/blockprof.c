// Cheap UC_HOOK_BLOCK profiler: counts entries per (address,size).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAP (1u << 20)
typedef struct { uint64_t key; uint64_t count; } slot;
static slot table[CAP];
static uint64_t total_entries, total_bytes;
static int enabled;

void bp_enable(int on) { enabled = on; }
void bp_reset(void) { memset(table, 0, sizeof table); total_entries = total_bytes = 0; }

void bp_visit(void *uc, uint64_t address, uint32_t size, void *user) {
    (void)uc; (void)user;
    if (!enabled) return;
    total_entries++;
    total_bytes += size;
    uint64_t key = (address << 16) | (size & 0xffff);
    uint32_t h = (uint32_t)((key * 0x9E3779B97F4A7C15ull) >> 44) & (CAP - 1);
    for (;;) {
        if (table[h].key == key) { table[h].count++; return; }
        if (table[h].key == 0) { table[h].key = key; table[h].count = 1; return; }
        h = (h + 1) & (CAP - 1);
    }
}

int bp_dump(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# entries=%llu bytes=%llu\n", (unsigned long long)total_entries, (unsigned long long)total_bytes);
    for (uint32_t i = 0; i < CAP; i++)
        if (table[i].key)
            fprintf(f, "%llx %llu %llu\n", (unsigned long long)(table[i].key >> 16),
                    (unsigned long long)(table[i].key & 0xffff), (unsigned long long)table[i].count);
    fclose(f);
    return 0;
}

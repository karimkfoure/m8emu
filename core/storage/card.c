#include "card.h"
#include "card_platform.h"
#include "storage.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CARD_SIZE ((size_t)128 * 1024 * 1024)

struct m8_card {
    m8_card_platform *files;
    int saved, failed;
    char error[512];
};
static int fail(m8_card *card, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(card->error, sizeof card->error, format, args);
    va_end(args);
    return -1;
}

/* Portable FAT16 formatter: the maintained 2 KiB clusters, one partition,
 * two FATs and 512 root entries. Only the volume label is allocated; the
 * original firmware creates its own factory files on first boot. */
typedef struct { unsigned char *metadata; size_t length; } empty_image;
static void put16(unsigned char *p, uint16_t value)
{
    p[0] = (unsigned char)value; p[1] = (unsigned char)(value >> 8);
}
static void put32(unsigned char *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++) p[i] = (unsigned char)(value >> (i * 8));
}
static int empty_read(void *context, size_t offset, unsigned char *out, size_t n)
{
    empty_image *image = context;
    memset(out, 0, n);
    if (offset < image->length) {
        size_t got = image->length - offset < n ? image->length - offset : n;
        memcpy(out, image->metadata + offset, got);
    }
    return 0;
}
static int empty_metadata(m8_card *card, size_t bytes, empty_image *result)
{
    const uint32_t total = (uint32_t)(bytes / 512), start = 64, part = total - start;
    uint32_t fat = 1, clusters = 0;
    unsigned attempts;
    for (attempts = 0; attempts < 32; attempts++) {
        clusters = (part - 8 - 2 * fat - 32) / 4;
        uint32_t need = ((clusters + 2) * 2 + 511) / 512;
        if (fat == need) break;
        fat = need;
    }
    if (attempts == 32 || clusters < 4085 || clusters >= 65525)
        return fail(card, "Cannot create valid FAT16 card geometry");
    uint32_t root = start + 8 + 2 * fat;
    empty_image image = {calloc((size_t)(root + 32), 512), (size_t)(root + 32) * 512};
    if (!image.metadata) return fail(card, "Cannot allocate FAT16 metadata");
    unsigned char *mbr = image.metadata, *vbr = mbr + start * 512;
    mbr[0x1be + 4] = 6; put32(mbr + 0x1be + 8, start); put32(mbr + 0x1be + 12, part);
    put16(mbr + 510, 0xaa55);
    memcpy(vbr, "\xeb\x3c\x90" "MSDOS5.0", 11);
    put16(vbr + 11, 512); vbr[13] = 4; put16(vbr + 14, 8); vbr[16] = 2;
    put16(vbr + 17, 512); vbr[21] = 0xf8; put16(vbr + 22, (uint16_t)fat);
    put16(vbr + 24, 32); put16(vbr + 26, 64); put32(vbr + 28, start); put32(vbr + 32, part);
    vbr[36] = 0x80; vbr[38] = 0x29; put32(vbr + 39, UINT32_C(0x4d384631));
    memcpy(vbr + 43, "M8         FAT16   ", 19); put16(vbr + 510, 0xaa55);
    for (unsigned i = 0; i < 2; i++) {
        unsigned char *table = mbr + (size_t)(start + 8 + i * fat) * 512;
        put16(table, 0xfff8); put16(table + 2, 0xffff);
    }
    unsigned char *label = mbr + (size_t)root * 512;
    memcpy(label, "M8         ", 11); label[11] = 8;
    put16(label + 14, 12u << 11); put16(label + 16, ((2026u - 1980u) << 9) | (9u << 5) | 16u);
    put16(label + 18, ((2026u - 1980u) << 9) | (9u << 5) | 16u);
    put16(label + 22, 12u << 11); put16(label + 24, ((2026u - 1980u) << 9) | (9u << 5) | 16u);
    *result = image;
    return 0;
}
int m8_card_empty_runtime(m8rt *rt)
{
    m8_card scratch = {0};
    empty_image image;
    const size_t size = 16 * 1024 * 1024;
    if (empty_metadata(&scratch, size, &image)) return -1;
    unsigned char *bytes = calloc(1, size);
    if (!bytes) { free(image.metadata); return -1; }
    memcpy(bytes, image.metadata, image.length);
    int result = m8rt_sd_replace_image(rt, bytes, size);
    free(bytes); free(image.metadata);
    return result;
}

m8_card *m8_card_open(const char *path, int read_only, int create,
                      char *error, size_t error_capacity)
{
    if (error && error_capacity) *error = 0;
    m8_card *card = calloc(1, sizeof *card);
    if (!card) {
        if (error && error_capacity) snprintf(error, error_capacity, "Cannot allocate card state");
        return NULL;
    }
    empty_image image = {0};
    if (create && empty_metadata(card, CARD_SIZE, &image)) goto failed;
    card->files = m8_card_platform_open(path, read_only, create, CARD_SIZE,
        empty_read, &image, card->error, sizeof card->error);
    free(image.metadata);
    if (card->files) return card;
failed:
    if (error && error_capacity) snprintf(error, error_capacity, "%s", card->error);
    m8_card_close(card);
    return NULL;
}
const char *m8_card_path(const m8_card *card)
{
    return card ? m8_card_platform_path(card->files) : NULL;
}
int m8_card_read_only(const m8_card *card)
{
    return !card || m8_card_platform_read_only(card->files);
}
const char *m8_card_error(const m8_card *card)
{
    return card ? card->error : "Card is absent";
}
int m8_card_protect_file(m8_card *card, const char *path)
{
    if (!card || card->failed) return -1;
    if (m8_card_platform_protect_file(card->files, path)) {
        card->failed = 1;
        return fail(card, "%s", m8_card_platform_error(card->files));
    }
    return 0;
}
static int runtime_read(void *context, size_t offset, unsigned char *out, size_t n)
{
    return m8rt_sd_read_image(context, offset, out, n);
}
int m8_card_save(m8_card *card, m8rt *runtime)
{
    if (!card) return -1;
    if (card->saved) return 0;
    if (card->failed) return -1;
    card->failed = 1; /* A publication failure is terminal; never replay it. */
    if (m8_card_read_only(card)) return fail(card, "Card is read-only: %s", m8_card_path(card));
    if (m8rt_sd_prepare_export(runtime, 1))
        return fail(card, "Card save refused: %s", m8rt_error(runtime));
    size_t size = m8rt_sd_image_size(runtime);
    if (m8_card_platform_save(card->files, size, runtime_read, runtime))
        return fail(card, "%s", m8_card_platform_error(card->files));
    card->saved = 1; card->failed = 0; card->error[0] = 0;
    return 0;
}
void m8_card_close(m8_card *card)
{
    if (!card) return;
    m8_card_platform_close(card->files);
    free(card);
}

int m8_card_session_load(m8_card *card,void **out,size_t *size) {
    if(!card)return -1;
    int result=m8_card_platform_session_load(card->files,out,size);
    if(result<0)fail(card,"%s",m8_card_platform_error(card->files));
    return result;
}
int m8_card_session_save(m8_card *card,const void *bytes,size_t size) {
    if(!card||card->failed)return -1;
    int result=m8_card_platform_session_save(card->files,bytes,size);
    if(result)fail(card,"%s",m8_card_platform_error(card->files));
    else card->error[0]=0;
    return result;
}
int m8_card_replace(m8_card *card,const void *bytes,size_t size,const void *media,size_t media_size) {
    if(!card||card->failed)return -1;
    if(m8_card_platform_replace(card->files,bytes,size,media,media_size))
        return fail(card,"%s",m8_card_platform_error(card->files));
    card->saved=0;
    snprintf(card->error,sizeof card->error,"%s",m8_card_platform_error(card->files));return 0;
}

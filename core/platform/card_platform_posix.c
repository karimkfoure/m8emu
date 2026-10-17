#define _POSIX_C_SOURCE 200809L
#include "card_platform.h"
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* POSIX adapter for Linux, macOS, and Android app-private files. */
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif

#define COPY_SIZE ((size_t)64 * 1024)

struct protected_file {
    char *path;
    struct stat identity;
    struct protected_file *next;
};

struct m8_card_platform {
    char *path, *backup, *directory, *lock;
    int lock_fd, source_fd, read_only;
    struct protected_file *protected_files;
    struct stat source_stat;
    char error[512];
};


static int fail(m8_card_platform *card, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(card->error, sizeof(card->error), format, args);
    va_end(args);
    return -1;
}
static char *append(const char *base, const char *suffix)
{
    size_t a = strlen(base), b = strlen(suffix);
    if (a > SIZE_MAX - b - 1) return NULL;
    char *result = malloc(a + b + 1);
    if (result) { memcpy(result, base, a); memcpy(result + a, suffix, b + 1); }
    return result;
}
static char *absolute_path(const char *path)
{
    char *expanded = NULL;
    const char *home = getenv("HOME");
    if (!path) {
        const char *data = getenv("XDG_DATA_HOME");
        if (data && *data) expanded = append(data, "/m8-live/card.img");
#ifdef __APPLE__
        else if (home && *home) expanded = append(home, "/Library/Application Support/m8-live/card.img");
#else
        else if (home && *home) expanded = append(home, "/.local/share/m8-live/card.img");
#endif
        else { errno = ENOENT; return NULL; }
    } else if (path[0] == '~' && (!path[1] || path[1] == '/')) {
        if (!home || !*home) { errno = ENOENT; return NULL; }
        expanded = append(home, path + 1);
    } else expanded = append(path, "");
    if (!expanded || expanded[0] == '/') return expanded;
    size_t cap = 256;
    char *cwd = NULL;
    for (;;) {
        char *next = realloc(cwd, cap);
        if (!next) { free(cwd); free(expanded); return NULL; }
        cwd = next;
        if (getcwd(cwd, cap)) break;
        if (errno != ERANGE || cap > SIZE_MAX / 2) { free(cwd); free(expanded); return NULL; }
        cap *= 2;
    }
    char *prefix = append(cwd, "/");
    char *result = prefix ? append(prefix, expanded) : NULL;
    free(prefix); free(cwd); free(expanded);
    return result;
}
static int regular_path(m8_card_platform *card, const char *path, int missing_ok)
{
    struct stat st;
    if (lstat(path, &st)) {
        if (missing_ok && errno == ENOENT) return 0;
        return fail(card, "Cannot inspect %s: %s", path, strerror(errno));
    }
    if (!S_ISREG(st.st_mode)) return fail(card, "Card file must be a regular file: %s", path);
    return 0;
}
static int make_directories(m8_card_platform *card)
{
    char *path = append(card->directory, "");
    if (!path) return fail(card, "Cannot allocate card directory path");
    for (char *p = path + 1;; p++) {
        if (*p && *p != '/') continue;
        char saved = *p; *p = 0;
        if (mkdir(path, 0700) && errno != EEXIST) {
            fail(card, "Cannot create %s: %s", path, strerror(errno));
            free(path); return -1;
        }
        struct stat st;
        if (stat(path, &st) || !S_ISDIR(st.st_mode)) {
            fail(card, "Card parent must be a directory: %s", path);
            free(path); return -1;
        }
        *p = saved;
        if (!saved) break;
    }
    free(path); return 0;
}
static int durable_directory(m8_card_platform *card)
{
    int fd = open(card->directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) return fail(card, "Cannot open card directory: %s", strerror(errno));
    int result = fsync(fd), error = errno;
    close(fd);
    /* Match the existing host policy where directory fsync is unsupported. */
    if (result && error != EINVAL && error != ENOTSUP)
        return fail(card, "Cannot sync card directory: %s", strerror(error));
    return 0;
}
static int fd_read(void *context, size_t offset, unsigned char *out, size_t n)
{
    int fd = *(int *)context;
    while (n) {
        ssize_t got = pread(fd, out, n, (off_t)offset);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) { if (!got) errno = EIO; return -1; }
        out += got; offset += (size_t)got; n -= (size_t)got;
    }
    return 0;
}
static int write_all(int fd, const unsigned char *data, size_t n)
{
    while (n) {
        ssize_t wrote = write(fd, data, n);
        if (wrote < 0 && errno == EINTR) continue;
        if (wrote <= 0) { if (!wrote) errno = EIO; return -1; }
        data += wrote; n -= (size_t)wrote;
    }
    return 0;
}
static void remove_stage(char **path)
{
    if (*path) { unlink(*path); free(*path); *path = NULL; }
}
static int stage_image(m8_card_platform *card, const char *target, size_t size,
                       m8_card_platform_reader reader, void *context, char **stage)
{
    int result = -1, fd = -1;
    unsigned char *data = NULL, *check = NULL;
    *stage = append(target, ".tmp.XXXXXX");
    if (!*stage) return fail(card, "Cannot allocate temporary card path");
    fd = mkstemp(*stage);
    if (fd < 0) { fail(card, "Cannot create temporary card: %s", strerror(errno)); goto done; }
    struct stat st;
    mode_t mode = 0600;
    if (!lstat(target, &st)) {
        if (!S_ISREG(st.st_mode)) { fail(card, "Card target must be a regular file: %s", target); goto done; }
        mode = st.st_mode & 0777;
    } else if (errno != ENOENT) { fail(card, "Cannot inspect card target: %s", strerror(errno)); goto done; }
    if (fchmod(fd, mode)) { fail(card, "Cannot set card file mode: %s", strerror(errno)); goto done; }
    data = malloc(COPY_SIZE); check = malloc(COPY_SIZE);
    if (!data || !check) { fail(card, "Cannot allocate card copy buffers"); goto done; }
    for (size_t offset = 0; offset < size;) {
        size_t n = size - offset < COPY_SIZE ? size - offset : COPY_SIZE;
        if (reader(context, offset, data, n)) {
            fail(card, "Cannot read card image at byte %zu", offset); goto done;
        }
        if (write_all(fd, data, n)) {
            fail(card, "Cannot stage card at byte %zu: %s", offset, strerror(errno)); goto done;
        }
        offset += n;
    }
    if (fsync(fd)) { fail(card, "Cannot sync card file: %s", strerror(errno)); goto done; }
    for (size_t offset = 0; offset < size;) {
        size_t n = size - offset < COPY_SIZE ? size - offset : COPY_SIZE;
        if (reader(context, offset, data, n)) {
            fail(card, "Cannot reread card image at byte %zu", offset); goto done;
        }
        if (fd_read(&fd, offset, check, n)) {
            fail(card, "Cannot verify card at byte %zu: %s", offset, strerror(errno)); goto done;
        }
        if (memcmp(data, check, n)) { fail(card, "Card verification failed at byte %zu", offset); goto done; }
        offset += n;
    }
    if (close(fd)) { fd = -1; fail(card, "Cannot close staged card: %s", strerror(errno)); goto done; }
    fd = -1; result = 0;
done:
    if (fd >= 0) close(fd);
    free(data); free(check);
    if (result) remove_stage(stage);
    return result;
}
static int protected_unchanged(m8_card_platform *card);

static int publish(m8_card_platform *card, char **stage, const char *target)
{
    if (protected_unchanged(card) || regular_path(card, target, 1)) return -1;
    if (rename(*stage, target)) return fail(card, "Cannot replace %s: %s", target, strerror(errno));
    free(*stage); *stage = NULL;
    return durable_directory(card);
}

m8_card_platform *m8_card_platform_open(const char *path, int read_only, int create, size_t initial_size,
    m8_card_platform_reader initial_reader, void *initial_context,
    char *error, size_t error_capacity)
{
    m8_card_platform *card = calloc(1, sizeof(*card));
    if (error && error_capacity) *error = 0;
    if (!card) {
        if (error && error_capacity) snprintf(error, error_capacity, "Cannot allocate card state");
        return NULL;
    }
    card->lock_fd = card->source_fd = -1;
    card->read_only = read_only != 0;
    card->path = absolute_path(path);
    if (!card->path) { fail(card, "Cannot resolve card path: %s", strerror(errno)); goto failed; }
    card->backup = append(card->path, ".bak");
    card->directory = append(card->path, "");
    if (!card->backup || !card->directory) { fail(card, "Cannot allocate card paths"); goto failed; }
    char *slash = strrchr(card->directory, '/');
    if (!slash || !slash[1]) { fail(card, "Card path must name a file"); goto failed; }
    if (slash == card->directory) slash[1] = 0; else *slash = 0;
    if (regular_path(card, card->path, create) || regular_path(card, card->backup, 1)
        || make_directories(card)) goto failed;
    card->lock = append(card->path, ".lock");
    if (!card->lock) { fail(card, "Cannot allocate card lock path"); goto failed; }
    card->lock_fd = open(card->lock, O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
    struct stat lock_stat;
    if (card->lock_fd < 0 || fstat(card->lock_fd, &lock_stat) || !S_ISREG(lock_stat.st_mode)) {
        fail(card, "Cannot open regular card lock: %s", strerror(errno)); goto failed;
    }
    if (flock(card->lock_fd, LOCK_EX | LOCK_NB)) { fail(card, "Card is in use: %s", card->path); goto failed; }
    /* Recheck after lock acquisition; another owner may have replaced it. */
    if (regular_path(card, card->path, create) || regular_path(card, card->backup, 1)) goto failed;
    if (lstat(card->path, &card->source_stat)) {
        if (errno != ENOENT || !create) { fail(card, "Card does not exist: %s", card->path); goto failed; }
        if (card->read_only) { fail(card, "Cannot create a read-only card: %s", card->path); goto failed; }
        if (!initial_reader || !initial_size) { fail(card, "Cannot create card without an initial image"); goto failed; }
        char *stage = NULL;
        int result = stage_image(card, card->path, initial_size, initial_reader, initial_context, &stage);
        if (!result) result = publish(card, &stage, card->path);
        remove_stage(&stage);
        if (result) goto failed;
    }
    card->source_fd = open(card->path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (card->source_fd < 0 || fstat(card->source_fd, &card->source_stat)
        || !S_ISREG(card->source_stat.st_mode) || card->source_stat.st_size <= 0
        || (uintmax_t)card->source_stat.st_size > SIZE_MAX) {
        fail(card, "Cannot open nonempty regular card: %s", card->path); goto failed;
    }
    card->read_only |= !(card->source_stat.st_mode & 0222) || access(card->path, W_OK) != 0;
    return card;
failed:
    if (error && error_capacity) snprintf(error, error_capacity, "%s", card->error);
    m8_card_platform_close(card); return NULL;
}
const char *m8_card_platform_path(const m8_card_platform *card) { return card ? card->path : NULL; }
int m8_card_platform_read_only(const m8_card_platform *card) { return !card || card->read_only; }
const char *m8_card_platform_error(const m8_card_platform *card) { return card ? card->error : "Card is absent"; }

static int source_unchanged(m8_card_platform *card)
{
    struct stat path, fd;
    if (lstat(card->path, &path) || fstat(card->source_fd, &fd))
        return fail(card, "Cannot inspect source card before save: %s", strerror(errno));
    struct stat *original = &card->source_stat;
    if (!S_ISREG(path.st_mode) || path.st_dev != original->st_dev || path.st_ino != original->st_ino
        || fd.st_size != original->st_size || fd.st_mtime != original->st_mtime
        || fd.st_ctime != original->st_ctime || !(fd.st_mode & 0222)
#if defined(__APPLE__)
        || fd.st_mtimespec.tv_nsec != original->st_mtimespec.tv_nsec
        || fd.st_ctimespec.tv_nsec != original->st_ctimespec.tv_nsec
#else
        || fd.st_mtim.tv_nsec != original->st_mtim.tv_nsec
        || fd.st_ctim.tv_nsec != original->st_ctim.tv_nsec
#endif
        )
        return fail(card, "Source card changed during the session; save refused: %s", card->path);
    return 0;
}

size_t m8_card_platform_size(const m8_card_platform *card)
{
    return card ? (size_t)card->source_stat.st_size : 0;
}
static int same_file(const struct stat *first, const struct stat *second)
{
    return first->st_dev == second->st_dev && first->st_ino == second->st_ino;
}
static int inspect_aliases(m8_card_platform *card, const struct stat *protected,
                           const char *protected_path)
{
    const char *paths[] = {card->path, card->backup, card->lock};
    const char *roles[] = {"source", "backup", "lock"};
    for (unsigned i = 0; i < sizeof paths / sizeof paths[0]; ++i) {
        struct stat st;
        if (lstat(paths[i], &st)) {
            if (i == 1 && errno == ENOENT) continue;
            return fail(card, "Cannot inspect card %s %s: %s", roles[i], paths[i], strerror(errno));
        }
        if (same_file(&st, protected))
            return fail(card, "Card %s aliases protected file %s: %s", roles[i], protected_path, paths[i]);
    }
    const int fds[] = {card->source_fd, card->lock_fd};
    const char *fd_roles[] = {"opened source", "opened lock"};
    for (unsigned i = 0; i < sizeof fds / sizeof fds[0]; ++i) {
        struct stat st;
        if (fstat(fds[i], &st))
            return fail(card, "Cannot inspect card %s: %s", fd_roles[i], strerror(errno));
        if (same_file(&st, protected))
            return fail(card, "Card %s aliases protected file: %s", fd_roles[i], protected_path);
    }
    return 0;
}
int m8_card_platform_protect_file(m8_card_platform *card, const char *path)
{
    if (!card || !path || !*path) return card ? fail(card, "Protected file path is empty") : -1;
    struct protected_file *file = calloc(1, sizeof *file);
    if (!file) return fail(card, "Cannot allocate protected file state");
    file->path = absolute_path(path);
    if (!file->path) { free(file); return fail(card, "Cannot resolve protected file path: %s", strerror(errno)); }
    int result = 0;
    if (stat(file->path, &file->identity))
        result = fail(card, "Cannot inspect protected file %s: %s", file->path, strerror(errno));
    else if (!S_ISREG(file->identity.st_mode))
        result = fail(card, "Protected input must be a regular file: %s", file->path);
    else result = inspect_aliases(card, &file->identity, file->path);
    if (result) { free(file->path); free(file); return -1; }
    file->next = card->protected_files;
    card->protected_files = file;
    return 0;
}
static int protected_unchanged(m8_card_platform *card)
{
    for (const struct protected_file *file = card->protected_files; file; file = file->next) {
        struct stat current;
        if (stat(file->path, &current))
            return fail(card, "Cannot inspect protected file before save %s: %s", file->path, strerror(errno));
        if (!same_file(&current, &file->identity))
            return fail(card, "Protected file changed during the session; save refused: %s", file->path);
        if (inspect_aliases(card, &file->identity, file->path)) return -1;
    }
    return 0;
}
int m8_card_platform_save(m8_card_platform *card, size_t size,
                         m8_card_platform_reader reader, void *context)
{
    if (!card || !reader) return -1;
    if (card->read_only) return fail(card, "Card is read-only: %s", card->path);
    if (source_unchanged(card) || protected_unchanged(card) || regular_path(card, card->backup, 1)) return -1;
    if (size != m8_card_platform_size(card))
        return fail(card, "Card size changed: source=%zu, session=%zu", m8_card_platform_size(card), size);
    char *current_stage = NULL, *backup_stage = NULL;
    int result = stage_image(card, card->path, size, reader, context, &current_stage);
    if (!result) result = stage_image(card, card->backup, size, fd_read, &card->source_fd, &backup_stage);
    if (!result) result = source_unchanged(card);
    if (!result) result = publish(card, &backup_stage, card->backup);
    if (!result) result = publish(card, &current_stage, card->path);
    remove_stage(&current_stage); remove_stage(&backup_stage);
    return result;
}
void m8_card_platform_close(m8_card_platform *card)
{
    if (!card) return;
    if (card->source_fd >= 0) close(card->source_fd);
    if (card->lock_fd >= 0) close(card->lock_fd);
    while (card->protected_files) {
        struct protected_file *file = card->protected_files;
        card->protected_files = file->next;
        free(file->path); free(file);
    }
    free(card->path); free(card->backup); free(card->directory); free(card->lock); free(card);
}

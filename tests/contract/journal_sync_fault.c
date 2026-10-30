#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Linux test-only interposer. Match final journal files, not atomic temp files.
int fsync(int descriptor) {
    char descriptor_path[64];
    char path[1024];
    (void)snprintf(descriptor_path, sizeof(descriptor_path), "/proc/self/fd/%d", descriptor);
    const ssize_t count = readlink(descriptor_path, path, sizeof(path) - 1);
    static unsigned journal_syncs = 0;
    const char* mode = getenv("ENGINE_JOURNAL_FAULT");
    if (count > 0 && mode != NULL) {
        path[count] = '\0';
        if (strstr(path, "live-commands-") != NULL && count >= 6 &&
            strcmp(path + count - 6, ".jsonl") == 0) {
            ++journal_syncs;
            const unsigned fail_on = strcmp(mode, "started") == 0     ? 1U
                                     : strcmp(mode, "completed") == 0 ? 2U
                                                                      : 0U;
            if (journal_syncs == fail_on) {
                errno = EIO;
                return -1;
            }
        }
    }
    void* symbol = dlsym(RTLD_NEXT, "fsync");
    int (*next_fsync)(int) = NULL;
    _Static_assert(sizeof(next_fsync) == sizeof(symbol), "POSIX function address size");
    memcpy(&next_fsync, &symbol, sizeof(next_fsync));
    if (next_fsync == NULL) {
        errno = EIO;
        return -1;
    }
    return next_fsync(descriptor);
}

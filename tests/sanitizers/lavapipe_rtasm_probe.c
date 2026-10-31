#define _GNU_SOURCE
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct mem_block {
    struct mem_block *next, *prev, *next_free, *prev_free, *heap;
    int ofs, size;
    unsigned flags;
};
// Diagnostic only: private entry points are valid solely for this exact ELF.
static int matching_build_id(uintptr_t base) {
    static const unsigned char expected[] = {0x75, 0xd1, 0x38, 0x0e, 0xbc, 0xd4, 0x89,
                                             0x14, 0xe4, 0x24, 0x8e, 0xb9, 0x8c, 0xd6,
                                             0x47, 0x40, 0xc2, 0xf1, 0x33, 0x0e};
    const Elf64_Ehdr* header = (const Elf64_Ehdr*)base;
    const Elf64_Phdr* programs = (const Elf64_Phdr*)(base + header->e_phoff);
    for (unsigned i = 0; i < header->e_phnum; ++i) {
        if (programs[i].p_type != PT_NOTE)
            continue;
        const unsigned char* cursor = (const unsigned char*)(base + programs[i].p_vaddr);
        const unsigned char* end = cursor + programs[i].p_memsz;
        while ((size_t)(end - cursor) >= sizeof(Elf64_Nhdr)) {
            const Elf64_Nhdr* note = (const Elf64_Nhdr*)cursor;
            const size_t name_size = (note->n_namesz + 3U) & ~3U;
            const size_t data_size = (note->n_descsz + 3U) & ~3U;
            cursor += sizeof(*note);
            if (name_size > (size_t)(end - cursor) ||
                data_size > (size_t)(end - cursor) - name_size)
                return 0;
            if (note->n_type == NT_GNU_BUILD_ID && note->n_namesz == 4 &&
                memcmp(cursor, "GNU", 4) == 0 && note->n_descsz == sizeof expected) {
                return memcmp(cursor + name_size, expected, sizeof expected) == 0;
            }
            cursor += name_size + data_size;
        }
    }
    return 0;
}
__attribute__((noinline)) static void own_leak_control(void) {
    void* p = malloc(73);
    if (!p)
        abort();
    fprintf(stderr, "Intentional application leak control: %p (73 bytes)\n", p);
}
int main(int argc, char** argv) {
    int keep = 0, own_leak = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--keep-loaded") == 0)
            keep = 1;
        else if (strcmp(argv[i], "--own-leak") == 0)
            own_leak = 1;
        else
            return 2;
    }
    void* driver =
        dlopen("/usr/lib/libvulkan_lvp.so", RTLD_NOW | RTLD_LOCAL | (keep ? RTLD_NODELETE : 0));
    if (!driver) {
        fprintf(stderr, "%s\n", dlerror());
        return 2;
    }
    struct link_map* map = NULL;
    if (dlinfo(driver, RTLD_DI_LINKMAP, &map) != 0)
        return 3;
    if (!matching_build_id((uintptr_t)map->l_addr)) {
        fputs("SKIPPED: driver Build ID does not match pinned diagnostic offsets\n", stderr);
        dlclose(driver);
        return 77;
    }
    // Source/disassembly correlation is recorded in M3_SANITIZER_DIAGNOSIS.md.
    uintptr_t alloc_address = (uintptr_t)map->l_addr + 0x2f8b20;
    uintptr_t free_address = (uintptr_t)map->l_addr + 0x2f8ca0;
    void* (*allocate)(size_t);
    void (*release)(void*);
    memcpy(&allocate, &alloc_address, sizeof allocate);
    memcpy(&release, &free_address, sizeof release);
    void* code = allocate(64);
    if (!code)
        return 4;
    release(code);
    struct mem_block* heap = *(struct mem_block**)((uintptr_t)map->l_addr + 0xe50ef0);
    printf("Mesa RTASM after allocate/free: heap=%p block=%p bytes=%zu+%zu free=%u size=%d\n",
           (void*)heap, (void*)heap->next, sizeof(*heap), sizeof(*heap->next),
           heap->next->flags & 1U, heap->next->size);
    fflush(stdout);
    if (heap->next->next != heap || (heap->next->flags & 1U) == 0)
        return 5;
    if (own_leak)
        own_leak_control();
    return dlclose(driver) != 0;
}

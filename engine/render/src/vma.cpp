// The Vulkan Memory Allocator is header-only. Its implementation is compiled
// exactly once, in this translation unit. volk.h must be included first so the
// implementation can import the volk function tables.
#include <volk.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

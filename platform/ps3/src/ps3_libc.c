// C11 functions PSL1GHT's newlib lacks (declared in platform/ps3/include/ps3_libc.h).
#include <malloc.h>

void* aligned_alloc(size_t alignment, size_t size) { return memalign(alignment, size); }

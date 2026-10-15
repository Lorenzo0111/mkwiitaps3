// Included before everything in the PS3's C and C++ (CMakeLists.txt).
// PSL1GHT's newlib is older than its libstdc++: the C99 parts libstdc++ found
// missing are switched on (CMakeLists.txt), and what newlib lacks is declared
// here so <cmath> can name it (the game never calls those) or defined in
// platform/ps3/src/ps3_libc.c.
#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef float float_t;
typedef double double_t;
long double log2l(long double);
long double logbl(long double);
double nexttoward(double, long double);
float nexttowardf(float, long double);
long double nexttowardl(long double, long double);
void* aligned_alloc(size_t alignment, size_t size);
#ifdef __cplusplus
}
// Kinoko reaches std::clamp through headers that include <algorithm> elsewhere.
#include <algorithm>
#endif

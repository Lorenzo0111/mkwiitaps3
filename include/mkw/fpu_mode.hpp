#pragma once
// The FPU mode (rounding, flush-to-zero) a worker thread copies from the
// thread that starts it, so both compute identical floats.
#include <cstdint>
#include <cstring>
namespace mkw {
#ifdef __PPU__
inline unsigned fpuMode(){double d;asm volatile("mffs %0":"=f"(d));std::uint64_t w;std::memcpy(&w,&d,8);return unsigned(w);}
inline void setFpuMode(unsigned mode){const std::uint64_t w=mode;double d;std::memcpy(&d,&w,8);asm volatile("mtfsf 0xff,%0"::"f"(d));}
#else
inline unsigned fpuMode(){return __builtin_arm_get_fpscr();}
inline void setFpuMode(unsigned mode){__builtin_arm_set_fpscr(mode);}
#endif
}

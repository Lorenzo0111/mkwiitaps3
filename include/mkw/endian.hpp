#pragma once
// Cooked game files (tools/cook_*.py) are little-endian: the 3DS and Vita
// read them in place. A big-endian CPU (the PS3's PPU) converts each field
// as it is loaded; on little-endian targets every helper here is a no-op.
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mkw::le {
inline constexpr bool Swap = std::endian::native == std::endian::big;
// n 4-byte words (u32, s32, float) at p, from file order to native.
inline void words(void* p, std::size_t n) {
	if constexpr (Swap) {
		auto* b = static_cast<std::uint8_t*>(p);
		for (std::size_t i = 0; i < n; ++i, b += 4) { std::uint32_t v; std::memcpy(&v, b, 4); v = __builtin_bswap32(v); std::memcpy(b, &v, 4); }
	} else { (void)p; (void)n; }
}
// n 2-byte values (u16, s16) at p.
inline void halves(void* p, std::size_t n) {
	if constexpr (Swap) {
		auto* b = static_cast<std::uint8_t*>(p);
		for (std::size_t i = 0; i < n; ++i, b += 2) { const std::uint8_t t = b[0]; b[0] = b[1]; b[1] = t; }
	} else { (void)p; (void)n; }
}
// Byte counts rounded down to whole values.
inline void wordBytes(void* p, std::size_t bytes) { words(p, bytes / 4); }
inline void halfBytes(void* p, std::size_t bytes) { halves(p, bytes / 2); }
// One value of any 2/4/8-byte arithmetic type, in place.
template <class T> inline void fix(T& v) {
	if constexpr (Swap && sizeof(T) > 1) {
		std::uint8_t b[sizeof(T)];
		std::memcpy(b, &v, sizeof(T));
		for (std::size_t i = 0; i < sizeof(T) / 2; ++i) { const std::uint8_t t = b[i]; b[i] = b[sizeof(T) - 1 - i]; b[sizeof(T) - 1 - i] = t; }
		std::memcpy(&v, b, sizeof(T));
	}
}
// Little-endian reads from bytes, on any CPU.
inline std::uint16_t u16(const void* p) { auto* b = static_cast<const std::uint8_t*>(p); return std::uint16_t(b[0] | b[1] << 8); }
inline std::uint32_t u32(const void* p) { auto* b = static_cast<const std::uint8_t*>(p); return std::uint32_t(b[0]) | std::uint32_t(b[1]) << 8 | std::uint32_t(b[2]) << 16 | std::uint32_t(b[3]) << 24; }
inline float f32(const void* p) { const std::uint32_t v = u32(p); float f; std::memcpy(&f, &v, 4); return f; }
}

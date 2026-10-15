// File access on the PS3. The game names its data folder as on the Vita
// (ux0:data/mkw); calls taking a path are wrapped at link time (-Wl,--wrap,
// CMakeLists.txt) and map that prefix onto the game's folder on the hard disk.
// fopen also counts opens and their time, as src/linear_lock.cpp did on the 3DS.
#include <3ds.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace mkw {std::atomic<unsigned> fileOpens{0};std::atomic<unsigned long long> fileOpenTicks{0};}

namespace {
constexpr const char* VitaRoot = "ux0:data";
// PARAM.SFO's title id is MKWP00001 (CMakeLists.txt): the installed package's own folder.
constexpr const char* Ps3Root = "/dev_hdd0/game/MKWP00001/USRDIR";
// path with the Vita prefix replaced; other paths are returned unchanged.
const char* mapped(const char* path, std::string& storage) {
	const size_t n = std::strlen(VitaRoot);
	if (!path || std::strncmp(path, VitaRoot, n) || (path[n] && path[n] != '/')) return path;
	storage = Ps3Root;
	storage += path + n;
	return storage.c_str();
}
}

extern "C" {
FILE* __real_fopen(const char* path, const char* mode);
FILE* __real_freopen(const char* path, const char* mode, FILE* stream);
int __real_mkdir(const char* path, mode_t mode);
int __real_rename(const char* from, const char* to);
int __real_remove(const char* path);
int __real_chdir(const char* path);

FILE* __wrap_fopen(const char* path, const char* mode) {
	const u64 start = svcGetSystemTick();
	std::string storage;
	FILE* file = __real_fopen(mapped(path, storage), mode);
	mkw::fileOpens.fetch_add(1, std::memory_order_relaxed);
	mkw::fileOpenTicks.fetch_add(svcGetSystemTick() - start, std::memory_order_relaxed);
	return file;
}
FILE* __wrap_freopen(const char* path, const char* mode, FILE* stream) {
	std::string storage;
	return __real_freopen(mapped(path, storage), mode, stream);
}
int __wrap_mkdir(const char* path, mode_t mode) {
	std::string storage;
	return __real_mkdir(mapped(path, storage), mode);
}
int __wrap_rename(const char* from, const char* to) {
	std::string a, b;
	return __real_rename(mapped(from, a), mapped(to, b));
}
int __wrap_remove(const char* path) {
	std::string storage;
	return __real_remove(mapped(path, storage));
}
int __wrap_chdir(const char* path) {
	std::string storage;
	return __real_chdir(mapped(path, storage));
}
}

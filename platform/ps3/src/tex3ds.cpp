// citro3d's .t3x loader (vendor/citro3d/source/tex3ds.c, zlib licence) for a
// big-endian CPU: its header is little-endian with bitfields, read here byte
// by byte. Only the in-memory import the game uses (Texture::loadMemory).
#include <tex3ds.h>
#include <cstdlib>
#include <cstring>

struct Tex3DS_Texture_s {
	u16 numSubTextures;
	u16 width;
	u16 height;
	u8 format;
	u8 mipmapLevels;
	Tex3DS_SubTexture subTextures[];
};

Tex3DS_Texture Tex3DS_TextureImport(const void* input, size_t insize, C3D_Tex* tex, C3D_TexCube* texcube, bool vram) {
	const auto* in = static_cast<const u8*>(input);
	auto u16le = [](const u8* p) { return u16(p[0] | p[1] << 8); };
	// u16 subtextures; u8 log2 width - 3 (bits 0-2), log2 height - 3 (3-5), type (6); u8 format; u8 mipmaps.
	if (insize < 5) return nullptr;
	const unsigned count = u16le(in);
	const unsigned sizes = in[2], format = in[3], mipmaps = in[4];
	size_t at = 5;
	if (insize - at < size_t(count) * 12) return nullptr;
	auto* texture = static_cast<Tex3DS_Texture>(std::malloc(sizeof(Tex3DS_Texture_s) + count * sizeof(Tex3DS_SubTexture)));
	if (!texture) return nullptr;
	texture->numSubTextures = u16(count);
	texture->width = u16(1u << ((sizes & 7) + 3));
	texture->height = u16(1u << (((sizes >> 3) & 7) + 3));
	texture->format = u8(format);
	texture->mipmapLevels = u8(mipmaps);
	for (unsigned i = 0; i < count; ++i, at += 12) {
		const u8* p = in + at;
		Tex3DS_SubTexture& s = texture->subTextures[i];
		s.width = u16le(p);
		s.height = u16le(p + 2);
		s.left = u16le(p + 4) / 1024.0f;
		s.top = u16le(p + 6) / 1024.0f;
		s.right = u16le(p + 8) / 1024.0f;
		s.bottom = u16le(p + 10) / 1024.0f;
	}
	C3D_TexInitParams params;
	params.width = texture->width;
	params.height = texture->height;
	params.maxLevel = texture->mipmapLevels;
	params.format = GPU_TEXCOLOR(texture->format);
	params.type = GPU_TEXTURE_MODE_PARAM((sizes >> 6) & 1);
	params.onVram = vram;
	if (!C3D_TexInitWithParams(tex, texcube, params)) {
		std::free(texture);
		return nullptr;
	}
	// The texture data follows, behind a compression header (decompress()).
	const size_t texsize = C3D_TexCalcTotalSize(tex->size, texture->mipmapLevels);
	if (!decompress(tex->data, texsize, nullptr, const_cast<u8*>(in + at), insize - at)) {
		C3D_TexDelete(tex);
		std::memset(tex, 0, sizeof *tex); // a failed import owns no storage
		std::free(texture);
		return nullptr;
	}
	// The caller (Texture::loadMemory) flushes, which converts it for the RSX.
	return texture;
}

void Tex3DS_TextureFree(Tex3DS_Texture texture) { std::free(texture); }

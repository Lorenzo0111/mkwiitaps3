// Shared between the citro3d-on-RSX files (c3d_gpu.cpp, c3d_shader.cpp).
#pragma once
#include <citro3d.h>
#include <rsx/rsx.h>
#include <cstdint>
#include <vector>

namespace c3dps3 {

extern gcmContextData* context;

// Vertex programs: NV40 ports of src/mkw.v.pica (DVLE 0) and
// src/effect.v.pica (DVLE 1).
enum : uint8_t { ProgramScene = 0, ProgramEffect = 1, ProgramCount = 2 };

// Everything that selects a generated fragment program. Zero-filled before
// use so it can be compared and hashed as bytes. opRgb/opAlpha are read
// through citro3d's bitfields: their place in opAll depends on byte order.
struct FragmentKey {
	uint16_t srcRgb[6], srcAlpha[6];
	uint16_t opRgb[6], opAlpha[6];
	uint16_t funcRgb[6], funcAlpha[6];
	uint16_t scaleRgb[6], scaleAlpha[6];
	uint32_t bufferUpdate; // texEnvBuf bits: 8-11 colour, 12-15 alpha (stages 0-3)
	uint8_t alphaFunc;     // GPU_TESTFUNC, or 0xFF with the alpha test off
	uint8_t units;         // bit n: texture unit n has a texture bound
	uint8_t projection0;   // unit 0 samples projectively (GPU_TEX_PROJECTION)
	uint8_t pad;
};

// An assembled RSX program: its header (rsxVertexProgram/rsxFragmentProgram)
// and microcode, as cgcomp lays them out. A fragment program's microcode is
// copied to RSX memory (ucodeOffset).
struct ProgramBlob {
	std::vector<uint8_t> bytes;
	void* ucode = nullptr;
	uint32_t ucodeOffset = 0;
};

// Fragment program units: the game's three texture units, and the unit
// that carries each draw's combiner constants (ConstantUnit).
constexpr int ConstantUnit = 7;
// Texels of a draw's constant row: the six combiner colours, the buffer
// colour and the alpha reference (in alpha).
constexpr int ConstantTexels = 8;

// Vertex program constant registers: c0-c24 are the PICA float uniforms
// (as C3D_FVUnif stores them, w first), c100-c101 literals.
constexpr int ConstLiterals = 100;
constexpr int SceneRegisters = 25;
constexpr int EffectRegisters = 4;

// c3d_shader.cpp
// Scene programs come in eight variants, one per combination of the boolean
// uniforms the PICA program branches on (bit 0 light texture/fog
// coordinate, bit 1 environment map, bit 2 diffuse lighting); skinning runs
// on the CPU (c3d_gpu.cpp).
const ProgramBlob& vertexProgram(uint8_t program, unsigned variant);
const ProgramBlob& fragmentProgram(const FragmentKey& key);
const ProgramBlob& presentVertexProgram();
const ProgramBlob& presentFragmentProgram();
int dvleIndex(const DVLE_s* dvle);
// Literal constants the vertex programs read (c100, c101), 8 floats.
const float* vertexLiterals();
// Programs assembled since the last reset, and their time.
extern unsigned programsCreated;
extern uint64_t programUs;

// rsx_asm.cpp: PSL1GHT's cgcomp assembler for NV40 program text.
bool assembleVertex(const char* text, std::vector<uint8_t>& out);
bool assembleFragment(const char* text, std::vector<uint8_t>& out);

// c3d_gpu.cpp
void* rsxAlloc(uint32_t alignment, uint32_t size);
uint64_t nowUs();

} // namespace c3dps3

// citro3d shaders on the RSX: NV40 ports of the game's PICA vertex shaders,
// libctru's shader binary API over them, and fragment programs generated
// from the PICA texture combiner (TEV) state with the semantics of the Vita
// port's GLSL generator (Azahar's): stage-0 "previous" reads source 3, there
// is a combiner buffer, every stage rounds to 8 bits, the alpha test compares
// 8-bit values. Program text is NV40 assembly, assembled at run time by
// PSL1GHT's cgcomp (rsx_asm.cpp).
#include "c3d_internal.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <map>
#include <string>

namespace c3dps3 {
namespace {

// ---- Uniform registers ------------------------------------------------------
// Each vertex program reads the PICA float uniform file as c0-c24 (C3D_FVec is
// stored w,z,y,x: PICA register i is c[i].wzyx). Registers follow
// src/mkw.v.pica's declaration order; boolean uniforms are 0x68 + their bit.
struct Uniform { const char* name; int reg; };
constexpr Uniform SceneUniforms[] = {
	{"projection", 0}, {"modelView", 4}, {"light", 8}, {"uvScroll", 9},
	{"envRow0", 10}, {"envRow1", 11}, {"envRow2", 12}, {"envParam", 13},
	{"uvRow0", 14}, {"uvRow1", 15}, {"uvRow2", 16}, {"uvRow3", 17}, {"tint", 18},
	{"uv1Row0", 19}, {"uv1Row1", 20}, {"lightDiff", 21}, {"lightDir", 22}, {"lightFn", 23},
	{"fogParam", 24}, {"bones", 25},
	{"retailLightTexture", 0x68 + 0}, {"environmentMapped", 0x68 + 1}, {"diffuseLighting", 0x68 + 2},
	{"skinned", 0x68 + 3}, {"skinnedNormals", 0x68 + 4},
};
constexpr Uniform EffectUniforms[] = {{"projection", 0}};

// c100 = (0, 1, 0.5, 1/255), c101 = (1e-12, 0, 0, 0).
const float Literals[8] = {0.f, 1.f, 0.5f, 1.f / 255.f, 1e-12f, 0.f, 0.f, 0.f};

// PICA clip z is in [-w, 0] and its depth is -z/w (1 near, 0 far; the game
// tests GEQUAL and clears to 0). The programs write z' = -z, and the
// viewport maps z'/w unchanged (c3d_gpu.cpp), so the depth buffer holds the
// PICA's own depth: tests and clears need no translation.
//
// Outputs: COL0 the vertex colour, TEX0 texture coordinate 0 (w for the
// projective lookup), TEX1 coordinates 1 (xy) and 2 (zw).
std::string sceneVertexText(unsigned variant) {
	const bool lightTexture = variant & 1, environment = variant & 2, diffuse = variant & 4;
	std::string s = "!!VP2.0\n";
	s += // p = (v0.xyz, 1); n = v1.xyz
		"MOV R0, v[0];\nMOV R0.w, c[100].y;\nMOV R1, v[1];\n"
		// mv = modelView * p
		"DP4 R2.x, c[4].wzyx, R0;\nDP4 R2.y, c[5].wzyx, R0;\nDP4 R2.z, c[6].wzyx, R0;\nDP4 R2.w, c[7].wzyx, R0;\n"
		// pos = projection * mv, depth -z/w
		"DP4 R3.x, c[0].wzyx, R2;\nDP4 R3.y, c[1].wzyx, R2;\nDP4 R3.z, c[2].wzyx, R2;\nDP4 R3.w, c[3].wzyx, R2;\n"
		"MOV R3.z, -R3.z;\nMOV o[HPOS], R3;\n"
		// t0 = (uvRow0.xyz . (uv0, 1), uvRow1.xyz . (uv0, 1), 0, 1) + uvScroll.xy
		"MOV R5, c[100].yyyy;\nMOV R5.xy, v[2].xyxy;\n"
		"DP3 R6.x, c[14].wzyy, R5;\nDP3 R6.y, c[15].wzyy, R5;\nMOV R6.zw, c[100].xxxy;\n"
		"ADD R6.xy, R6, c[9].wzwz;\nMOV o[TEX0], R6;\n"
		// t1 from (uv1, 1) and uvRow2/3, + uvScroll.zw
		"MOV R5.xy, v[2].zwzw;\n"
		"DP3 R7.x, c[16].wzyy, R5;\nDP3 R7.y, c[17].wzyy, R5;\nMOV R7.zw, c[100].xxxy;\n"
		"ADD R7.xy, R7, c[9].yxyx;\n";
	if (environment)
		s += // m = normalize(envRows . n) (length at least 1/255)
			"DP3 R8.x, c[10].wzyy, R1;\nDP3 R8.y, c[11].wzyy, R1;\nDP3 R8.z, c[12].wzyy, R1;\n"
			"DP3 R9.x, R8, R8;\nMAX R9.x, R9.x, c[100].w;\nRSQ R9.x, R9.x;\nMUL R8.xyz, R8, R9.x;\n"
			// r8 = 0.5 + 0.5 * (e.x * m.x + e.y * m.z, m.y); t1.xy += (r8 - t1.xy) * e.z
			"MUL R10.x, c[13].w, R8.x;\nMAD R10.x, c[13].z, R8.z, R10.x;\nMOV R10.y, R8.y;\n"
			"MAD R10.xy, R10, c[100].z, c[100].z;\nADD R10.xy, R10, -R7;\nMUL R10.xy, R10, c[13].y;\n"
			"ADD R7.xy, R7, R10;\n";
	s += // uv1 = (uv1Row0 . t1, uv1Row1 . t1)
		"DP4 R11.x, c[19].wzyx, R7;\nDP4 R11.y, c[20].wzyx, R7;\n"
		"MOV R13, c[8].wzyx;\n";
	if (diffuse)
		s += // r1 = normalize(modelView.xyz . n); d = max(fn.z, fn.x * (lightDir.xyz . r1) + fn.y)
			"DP3 R12.x, c[4].wzyy, R1;\nDP3 R12.y, c[5].wzyy, R1;\nDP3 R12.z, c[6].wzyy, R1;\n"
			"DP3 R9.x, R12, R12;\nMAX R9.x, R9.x, c[100].w;\nRSQ R9.x, R9.x;\nMUL R12.xyz, R12, R9.x;\n"
			"DP3 R9.y, c[22].wzyy, R12;\nMAD R9.y, c[23].w, R9.y, c[23].z;\nMAX R9.y, R9.y, c[23].y;\n"
			// col = light + lightDiff * d
			"MAD R13, c[21].wzyx, R9.y, R13;\n";
	s += // col = min(lightDir.w, col); colour = clamp(v3 / 255 * col * tint, 0, 1)
		"MIN R13, R13, c[22].x;\n"
		"MUL R14, v[3], c[100].w;\nMUL R14, R14, R13;\nMUL R14, R14, c[18].wzyx;\n"
		"MAX R14, R14, c[100].x;\nMIN R14, R14, c[100].y;\nMOV o[COL0], R14;\n";
	if (lightTexture) s += "MOV R11.zw, R11.xxxy;\n";
	else // t2 = (fog.x * mv.z + fog.y, 0.5)
		s += "MAD R15.x, c[24].w, R2.z, c[24].z;\nMOV R15.y, c[100].z;\nMOV R11.zw, R15.xxxy;\n";
	s += "MOV o[TEX1], R11;\nEND\n";
	return s;
}

const char* EffectVertexText =
	"!!VP2.0\n"
	"MOV R0, v[0];\nMOV R0.w, c[100].y;\n"
	"DP4 R1.x, c[0].wzyx, R0;\nDP4 R1.y, c[1].wzyx, R0;\nDP4 R1.z, c[2].wzyx, R0;\nDP4 R1.w, c[3].wzyx, R0;\n"
	"MOV R1.z, -R1.z;\nMOV o[HPOS], R1;\n"
	// t0 = (v1.x, v1.y, 0, v1.z); t1 = v2.xy; t2 = v2.zw
	"MOV R2, c[100].xxxx;\nMOV R2.xyw, v[1].xyzz;\nMOV o[TEX0], R2;\n"
	"MOV o[TEX1], v[2];\n"
	"MUL R3, v[3], c[100].w;\nMAX R3, R3, c[100].x;\nMIN R3, R3, c[100].y;\nMOV o[COL0], R3;\n"
	"END\n";

// The presentation pass: a target's texture onto the display.
const char* PresentVertexText =
	"!!VP2.0\n"
	"MOV R0, c[100].xxxy;\nMOV R0.xy, v[0].xyxy;\nMOV o[HPOS], R0;\n"
	"MOV R1, c[100].xxxy;\nMOV R1.xy, v[1].xyxy;\nMOV o[TEX0], R1;\n"
	"END\n";
const char* PresentFragmentText =
	"!!FP2.0\n"
	"#var float4 uv : $vin.TEX0 : TEX0 : -1 : 1\n"
	"#const c[0] = 1 1 1 1\n"
	"MOV R1, f[TEX0];\n"
	"TEX R0, R1, texture[0], 2D;\n"
	"MOV R0.w, c[0].x;\n"
	"MOV o[COLR], R0;\n"
	"END\n";

// ---- Fragment generator ------------------------------------------------------
// Registers: R0 primary colour (8-bit), R1-R3 texture units 0-2, R4 combiner
// buffer, R5 next buffer, R6 combiner output, R7 zero, R8 the stage's
// constant, R9-R11 operands, R12 stage result, R13-R14 scratch.
// Literals (one per instruction at most: the RSX reads a single inline
// constant): c0 = (255, 0.5, 1/255, 1), c1 = (0, 2, 4, 8), c2-c9 texel i of
// the constant row ((i + 0.5) / 8, 0.5).
std::string reg(int source) {
	switch (source) {
	case GPU_PRIMARY_COLOR: return "R0";
	case GPU_TEXTURE0: return "R1";
	case GPU_TEXTURE1: return "R2";
	case GPU_TEXTURE2: return "R3";
	case GPU_PREVIOUS_BUFFER: return "R4";
	case GPU_CONSTANT: return "R8";
	case GPU_PREVIOUS: return "R6";
	default: return "R7"; // fragment lighting and unit 3 are unused
	}
}
int sourceOf(const FragmentKey& k, int stage, int which, bool alpha) {
	const uint16_t sources = alpha ? k.srcAlpha[stage] : k.srcRgb[stage];
	int s = (sources >> (4 * which)) & 0xF;
	// Stage 0 has no previous output: the hardware reads its third source instead.
	if (stage == 0 && s == GPU_PREVIOUS) s = (sources >> 8) & 0xF;
	return s;
}
// Operand `which` of a stage into R9 + which (.xyz for colour, .w for alpha).
void operand(std::string& s, const FragmentKey& k, int stage, int which, bool alpha) {
	const std::string r = reg(sourceOf(k, stage, which, alpha));
	const std::string dst = "R" + std::to_string(9 + which) + (alpha ? ".w" : ".xyz");
	if (alpha) {
		static const char* const component[4] = {".w", ".x", ".y", ".z"};
		const unsigned op = (k.opAlpha[stage] >> (4 * which)) & 0x7;
		if (op & 1) s += "ADD " + dst + ", -" + r + component[op >> 1] + ", c[0].w;\n";
		else s += "MOV " + dst + ", " + r + component[op >> 1] + ";\n";
		return;
	}
	switch ((k.opRgb[stage] >> (4 * which)) & 0xF) {
	case 0x0: s += "MOV " + dst + ", " + r + ";\n"; break;
	case 0x1: s += "ADD " + dst + ", -" + r + ", c[0].w;\n"; break;
	case 0x2: s += "MOV " + dst + ", " + r + ".wwww;\n"; break;
	case 0x3: s += "ADD " + dst + ", -" + r + ".wwww, c[0].w;\n"; break;
	case 0x4: s += "MOV " + dst + ", " + r + ".xxxx;\n"; break;
	case 0x5: s += "ADD " + dst + ", -" + r + ".xxxx, c[0].w;\n"; break;
	case 0x8: s += "MOV " + dst + ", " + r + ".yyyy;\n"; break;
	case 0x9: s += "ADD " + dst + ", -" + r + ".yyyy, c[0].w;\n"; break;
	case 0xC: s += "MOV " + dst + ", " + r + ".zzzz;\n"; break;
	case 0xD: s += "ADD " + dst + ", -" + r + ".zzzz, c[0].w;\n"; break;
	default: s += "MOV " + dst + ", R7;\n"; break;
	}
}
// R12.m = func(R9, R10, R11), m being .xyz (colour) or .w (alpha).
void combine(std::string& s, int func, bool vector) {
	const std::string m = vector ? ".xyz" : ".w";
	const std::string d = "R12" + m;
	switch (func) {
	case GPU_REPLACE: s += "MOV " + d + ", R9;\n"; break;
	case GPU_MODULATE: s += "MUL " + d + ", R9, R10;\n"; break;
	case GPU_ADD: s += "ADD " + d + ", R9, R10;\n"; break;
	case GPU_ADD_SIGNED: s += "ADD " + d + ", R9, R10;\nADD " + d + ", R12, -c[0].y;\n"; break;
	case GPU_INTERPOLATE: // mix(b, a, c) = b + (a - b) * c
		s += "ADD R13" + m + ", R9, -R10;\nMAD " + d + ", R13, R11, R10;\n"; break;
	case GPU_SUBTRACT: s += "ADD " + d + ", R9, -R10;\n"; break;
	case GPU_DOT3_RGB:
	case GPU_DOT3_RGBA:
		if (vector) s += "ADD R13.xyz, R9, -c[0].y;\nADD R14.xyz, R10, -c[0].y;\nDP3 R12.x, R13, R14;\nMUL R12.xyz, R12.xxxx, c[1].z;\n";
		else s += "MOV R12.w, R9;\n";
		break;
	case GPU_MULTIPLY_ADD: s += "MAD " + d + ", R9, R10, R11;\n"; break;
	case GPU_ADD_MULTIPLY: s += "ADD R13" + m + ", R9, R10;\nMIN R13" + m + ", R13, c[0].w;\nMUL " + d + ", R13, R11;\n"; break;
	default: s += "MOV " + d + ", R7;\n"; break;
	}
}
// x = floor(clamp(x, 0, 1) * 255 + 0.5) / 255 on a mask.
void round8(std::string& s, const std::string& r, const std::string& m) {
	s += "MOV_SAT " + r + m + ", " + r + ";\n";
	s += "MAD " + r + m + ", " + r + ", c[0].x, c[0].y;\nFLR " + r + m + ", " + r + ";\nMUL " + r + m + ", " + r + ", c[0].z;\n";
}
std::string texel(int i) { return "c[" + std::to_string(2 + i) + "]"; }

bool passThrough(const FragmentKey& k, int i) {
	return k.funcRgb[i] == GPU_REPLACE && k.funcAlpha[i] == GPU_REPLACE &&
		(k.srcRgb[i] & 0xF) == GPU_PREVIOUS && (k.srcAlpha[i] & 0xF) == GPU_PREVIOUS &&
		(k.opRgb[i] & 0xF) == 0 && (k.opAlpha[i] & 0x7) == 0 && !k.scaleRgb[i] && !k.scaleAlpha[i] && i;
}

std::string fragmentText(const FragmentKey& k) {
	std::string s = "!!FP2.0\n";
	s += "#var float4 tc0 : $vin.TEX0 : TEX0 : -1 : 1\n#var float4 tc12 : $vin.TEX1 : TEX1 : -1 : 1\n";
	s += "#const c[0] = 255 0.5 0.00392156863 1\n#const c[1] = 0 2 4 8\n";
	for (int i = 0; i < ConstantTexels; ++i) {
		char line[64];
		std::snprintf(line, sizeof line, "#const c[%d] = %.9g 0.5 0 0\n", 2 + i, (i + 0.5) / ConstantTexels);
		s += line;
	}
	// Which stages run, and whether any reads the combiner buffer.
	bool usesBuffer = false;
	for (int i = 0; i < 6; ++i) {
		if (passThrough(k, i)) continue;
		for (int w = 0; w < 3; ++w)
			usesBuffer |= sourceOf(k, i, w, false) == GPU_PREVIOUS_BUFFER || sourceOf(k, i, w, true) == GPU_PREVIOUS_BUFFER;
	}
	s += "MOV R7, c[1].xxxx;\nMOV R6, R7;\nMOV R4, R7;\n";
	s += "MOV R0, f[COL0];\n";
	round8(s, "R0", "");
	if (k.units & 1) {
		s += "MOV R13, f[TEX0];\n";
		if (k.projection0) s += "RCP R14.x, R13.w;\nMUL R13.xy, R13, R14.x;\n";
		s += "TEX R1, R13, texture[0], 2D;\n";
	} else s += "MOV R1, R7;\n";
	if (k.units & 2) s += "MOV R13, f[TEX1];\nTEX R2, R13, texture[1], 2D;\n";
	else s += "MOV R2, R7;\n";
	if (k.units & 4) s += "MOV R13, f[TEX1].zwzw;\nTEX R3, R13, texture[2], 2D;\n";
	else s += "MOV R3, R7;\n";
	if (usesBuffer) s += "TEX R5, " + texel(6) + ", texture[" + std::to_string(ConstantUnit) + "], 2D;\n";
	for (int i = 0; i < 6; ++i) {
		if (!passThrough(k, i)) {
			bool konst = false;
			for (int w = 0; w < 3; ++w) konst |= sourceOf(k, i, w, false) == GPU_CONSTANT || sourceOf(k, i, w, true) == GPU_CONSTANT;
			if (konst) s += "TEX R8, " + texel(i) + ", texture[" + std::to_string(ConstantUnit) + "], 2D;\n";
			for (int w = 0; w < 3; ++w) operand(s, k, i, w, false);
			combine(s, k.funcRgb[i], true);
			round8(s, "R12", ".xyz");
			if (k.funcRgb[i] == GPU_DOT3_RGBA) s += "MOV R12.w, R12.x;\n";
			else {
				for (int w = 0; w < 3; ++w) operand(s, k, i, w, true);
				combine(s, k.funcAlpha[i], false);
				round8(s, "R12", ".w");
			}
			static const char* const scale[4] = {"", "c[1].y", "c[1].z", "c[1].w"};
			if (k.scaleRgb[i] & 3) s += std::string("MUL_SAT R6.xyz, R12, ") + scale[k.scaleRgb[i] & 3] + ";\n";
			else s += "MOV R6.xyz, R12;\n";
			if (k.scaleAlpha[i] & 3) s += std::string("MUL_SAT R6.w, R12, ") + scale[k.scaleAlpha[i] & 3] + ";\n";
			else s += "MOV R6.w, R12;\n";
		}
		if (usesBuffer) {
			s += "MOV R4, R5;\n";
			if (i < 4 && (k.bufferUpdate >> (8 + i) & 1)) s += "MOV R5.xyz, R6;\n";
			if (i < 4 && (k.bufferUpdate >> (12 + i) & 1)) s += "MOV R5.w, R6;\n";
		}
	}
	if (k.alphaFunc != 0xFF && k.alphaFunc != GPU_ALWAYS) {
		if (k.alphaFunc == GPU_NEVER) s += "KIL TR.x;\n";
		else {
			// The alpha and the reference (texel 7's alpha) as 0-255 integers.
			s += "TEX R14, " + texel(7) + ", texture[" + std::to_string(ConstantUnit) + "], 2D;\n";
			s += "MAD R13.x, R6.w, c[0].x, c[0].y;\nFLR R13.x, R13.x;\n";
			s += "MAD R14.x, R14.w, c[0].x, c[0].y;\nFLR R14.x, R14.x;\n";
			s += "ADDRC RC.x, R13.x, -R14.x;\n";
			const char* discard = "NE";
			switch (k.alphaFunc) {
			case GPU_EQUAL: discard = "NE"; break;    // pass a == ref
			case GPU_NOTEQUAL: discard = "EQ"; break; // pass a != ref
			case GPU_LESS: discard = "GE"; break;     // pass a < ref
			case GPU_LEQUAL: discard = "GT"; break;   // pass a <= ref
			case GPU_GREATER: discard = "LE"; break;  // pass a > ref
			case GPU_GEQUAL: discard = "LT"; break;   // pass a >= ref
			}
			s += std::string("KIL ") + discard + ".x;\n";
		}
	}
	s += "MOV o[COLR], R6;\nEND\n";
	return s;
}

ProgramBlob makeVertex(const std::string& text, const char* what) {
	ProgramBlob p;
	if (!assembleVertex(text.c_str(), p.bytes)) {
		std::printf("c3dps3: %s vertex program failed to assemble\n%s\n", what, text.c_str());
		std::abort();
	}
	return p;
}
// A fragment program's microcode lives in RSX memory.
ProgramBlob makeFragment(const std::string& text, const char* what) {
	ProgramBlob p;
	if (!assembleFragment(text.c_str(), p.bytes)) {
		std::printf("c3dps3: %s fragment program failed to assemble\n%s\n", what, text.c_str());
		std::abort();
	}
	const auto* fp = reinterpret_cast<const rsxFragmentProgram*>(p.bytes.data());
	const uint32_t size = uint32_t(p.bytes.size() - fp->ucode_off);
	p.ucode = rsxAlloc(64, size);
	if (!p.ucode) { std::printf("c3dps3: out of RSX memory for a fragment program\n"); std::abort(); }
	std::memcpy(p.ucode, p.bytes.data() + fp->ucode_off, size);
	rsxAddressToOffset(p.ucode, &p.ucodeOffset);
	return p;
}

struct KeyLess {
	bool operator()(const FragmentKey& a, const FragmentKey& b) const { return std::memcmp(&a, &b, sizeof a) < 0; }
};
std::map<FragmentKey, ProgramBlob, KeyLess> fragmentPrograms;

} // namespace

unsigned programsCreated = 0;
uint64_t programUs = 0;

const float* vertexLiterals() { return Literals; }

const ProgramBlob& vertexProgram(uint8_t program, unsigned variant) {
	static ProgramBlob scene[8], effect;
	static bool sceneReady[8] = {}, effectReady = false;
	if (program == ProgramEffect) {
		if (!effectReady) { effect = makeVertex(EffectVertexText, "effect"); effectReady = true; }
		return effect;
	}
	variant &= 7;
	if (!sceneReady[variant]) { scene[variant] = makeVertex(sceneVertexText(variant), "scene"); sceneReady[variant] = true; }
	return scene[variant];
}

const ProgramBlob& fragmentProgram(const FragmentKey& key) {
	auto it = fragmentPrograms.find(key);
	if (it != fragmentPrograms.end()) return it->second;
	const uint64_t start = nowUs();
	ProgramBlob p = makeFragment(fragmentText(key), "combiner");
	++programsCreated;
	programUs += nowUs() - start;
	return fragmentPrograms.emplace(key, std::move(p)).first->second;
}

const ProgramBlob& presentVertexProgram() {
	static ProgramBlob p = makeVertex(PresentVertexText, "present");
	return p;
}
const ProgramBlob& presentFragmentProgram() {
	static ProgramBlob p = makeFragment(PresentFragmentText, "present");
	return p;
}

// The fake DVLB has one DVLE per vertex program; a DVLE's index is its
// position in that array.
static DVLB_s* parsed = nullptr;
int dvleIndex(const DVLE_s* dvle) {
	if (!parsed || !dvle) return ProgramScene;
	const int index = int(dvle - parsed->DVLE);
	return index >= 0 && index < int(parsed->numDVLE) ? index : ProgramScene;
}

} // namespace c3dps3

using namespace c3dps3;

// ---- libctru shader binary API ---------------------------------------------------
// The game embeds a PICA binary (build/shader_data.hpp); here it is ignored and
// DVLE 0/1 name the scene and effect programs above.
DVLB_s* DVLB_ParseFile(u32*, u32) {
	auto* dvlb = static_cast<DVLB_s*>(std::calloc(1, sizeof(DVLB_s)));
	dvlb->numDVLE = ProgramCount;
	dvlb->DVLE = static_cast<DVLE_s*>(std::calloc(ProgramCount, sizeof(DVLE_s)));
	parsed = dvlb;
	return dvlb;
}
void DVLB_Free(DVLB_s* dvlb) {
	if (!dvlb) return;
	if (parsed == dvlb) parsed = nullptr;
	std::free(dvlb->DVLE);
	std::free(dvlb);
}
s8 DVLE_GetUniformRegister(DVLE_s* dvle, const char* name) {
	const int program = dvleIndex(dvle);
	const Uniform* table = program == ProgramScene ? SceneUniforms : EffectUniforms;
	const size_t count = program == ProgramScene ? std::size(SceneUniforms) : std::size(EffectUniforms);
	for (size_t i = 0; i < count; ++i)
		if (!std::strcmp(table[i].name, name)) return s8(table[i].reg);
	return -1;
}
Result shaderInstanceInit(shaderInstance_s* si, DVLE_s* dvle) {
	std::memset(si, 0, sizeof *si);
	si->dvle = dvle;
	return 0;
}
Result shaderInstanceFree(shaderInstance_s* si) { std::free(si); return 0; }
s8 shaderInstanceGetUniformLocation(shaderInstance_s* si, const char* name) {
	if (!si) return -1;
	return DVLE_GetUniformRegister(si->dvle, name);
}
Result shaderProgramInit(shaderProgram_s* sp) { std::memset(sp, 0, sizeof *sp); return 0; }
Result shaderProgramFree(shaderProgram_s* sp) {
	if (sp->vertexShader) shaderInstanceFree(sp->vertexShader);
	std::memset(sp, 0, sizeof *sp);
	return 0;
}
Result shaderProgramSetVsh(shaderProgram_s* sp, DVLE_s* dvle) {
	if (sp->vertexShader) shaderInstanceFree(sp->vertexShader);
	sp->vertexShader = static_cast<shaderInstance_s*>(std::calloc(1, sizeof(shaderInstance_s)));
	return shaderInstanceInit(sp->vertexShader, dvle);
}

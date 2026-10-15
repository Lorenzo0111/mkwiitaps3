// NV40 vertex and fragment program text to RSX programs, at run time, with
// the assembler of PSL1GHT's cgcomp (vendor/psl1ght/tools/cgcomp, MIT). The
// result is laid out as cgcomp's .vpo/.fpo files (tools/cgcomp/source/main.cpp),
// which PSL1GHT's rsxLoadVertexProgram/rsxLoadFragmentProgramLocation read.
// cgcomp's headers declare their own copies of the rsx_program.h structures,
// so this file includes no PSL1GHT header.
#include "types.h"
#include "fpparser.h"
#include "vpparser.h"
#include "compilervp.h"
#include "compilerfp.h"
#include <cstdint>
#include <cstring>
#include <vector>

namespace c3dps3 {

namespace {
// Instruction lines in the text (everything after the !! header but blank
// lines, comments and declarations): cgcomp skips a mnemonic it does not
// know, so a count that differs from what it parsed means a typo.
int instructionLines(const char* text) {
	int count = 0;
	bool inProgram = false;
	for (const char* line = text; *line;) {
		const char* end = std::strchr(line, '\n');
		if (!end) end = line + std::strlen(line);
		const char* p = line;
		while (p < end && (*p == ' ' || *p == '\t')) ++p;
		if (p < end && *p != '#') {
			if (!inProgram) inProgram = p[0] == '!' && p[1] == '!';
			else if (std::strncmp(p, "OPTION", 6) && std::strncmp(p, "PARAM", 5) && std::strncmp(p, "TEMP", 4)) ++count;
		}
		line = *end ? end + 1 : end;
	}
	return count;
}
template <class T> T* at(std::vector<uint8_t>& out, size_t offset) { return reinterpret_cast<T*>(out.data() + offset); }
size_t align(std::vector<uint8_t>& out, size_t to) {
	while (out.size() % to) out.push_back(0);
	return out.size();
}
}

bool assembleVertex(const char* text, std::vector<uint8_t>& out) {
	std::vector<char> source(text, text + std::strlen(text) + 1);
	CVPParser parser;
	CCompilerVP compiler;
	parser.Parse(source.data());
	if (parser.GetInstructionCount() != instructionLines(text)) return false;
	compiler.Compile(&parser);

	struct vertex_program_exec* vpi = compiler.GetInstructions();
	for (const auto& reloc : compiler.GetBranchRelocations()) {
		struct vertex_program_exec* vpe = &vpi[reloc.location];
		vpe->data[3] &= ~NV40_VP_INST_IADDRL_MASK;
		vpe->data[3] |= (reloc.target & 7) << NV40_VP_INST_IADDRL_SHIFT;
		vpe->data[2] &= ~NV40_VP_INST_IADDRH_MASK;
		vpe->data[2] |= ((reloc.target >> 3) & 0x3f) << NV40_VP_INST_IADDRH_SHIFT;
	}
	for (const auto& reloc : compiler.GetConstRelocations()) {
		struct vertex_program_exec* vpe = &vpi[reloc.location];
		vpe->data[1] &= ~NVFX_VP(INST_CONST_SRC_MASK);
		vpe->data[1] |= reloc.target << NVFX_VP(INST_CONST_SRC_SHIFT);
	}

	// No attribute or constant tables: the RSX layer binds attributes by
	// register and uploads every constant itself.
	out.assign(sizeof(rsxVertexProgram), 0);
	const size_t attr = align(out, 4);
	const size_t constants = align(out, 4);
	const size_t ucode = align(out, 16);
	rsxVertexProgram* vp = at<rsxVertexProgram>(out, 0);
	vp->magic = ('V' << 8) | 'P';
	vp->num_regs = u16(compiler.GetNumRegs());
	vp->input_mask = compiler.GetInputMask();
	vp->output_mask = compiler.GetOutputMask();
	vp->attr_off = u32(attr);
	vp->const_off = u32(constants);
	vp->ucode_off = u32(ucode);
	vp->num_insn = u16(compiler.GetInstructionCount());
	for (int i = 0; i < compiler.GetInstructionCount(); ++i)
		for (int w = 0; w < 4; ++w) {
			const u32 word = vpi[i].data[w];
			const uint8_t bytes[4] = {uint8_t(word >> 24), uint8_t(word >> 16), uint8_t(word >> 8), uint8_t(word)};
			out.insert(out.end(), bytes, bytes + 4);
		}
	return true;
}

bool assembleFragment(const char* text, std::vector<uint8_t>& out) {
	std::vector<char> source(text, text + std::strlen(text) + 1);
	CFPParser parser;
	CCompilerFP compiler;
	parser.Parse(source.data());
	if (parser.GetInstructionCount() != instructionLines(text)) return false;
	compiler.Compile(&parser);
	if (!compiler.GetConstRelocations().empty()) return false; // only inline (#const) literals

	out.assign(sizeof(rsxFragmentProgram), 0);
	const size_t attr = align(out, 4);
	const size_t constants = align(out, 4);
	const size_t ucode = align(out, 16);
	rsxFragmentProgram* fp = at<rsxFragmentProgram>(out, 0);
	fp->magic = ('F' << 8) | 'P';
	fp->num_regs = u16(compiler.GetNumRegs());
	fp->fp_control = u32(compiler.GetFPControl());
	fp->texcoords = u16(compiler.GetTexcoords());
	fp->texcoord2D = u16(compiler.GetTexcoord2D());
	fp->texcoord3D = u16(compiler.GetTexcoord3D());
	fp->attr_off = u32(attr);
	fp->const_off = u32(constants);
	fp->ucode_off = u32(ucode);
	fp->num_insn = u16(compiler.GetInstructionCount());
	// The fragment unit reads each word with its 16-bit halves swapped.
	const struct fragment_program_exec* fpi = compiler.GetInstructions();
	for (int i = 0; i < compiler.GetInstructionCount(); ++i)
		for (int w = 0; w < 4; ++w) {
			const u32 word = (fpi[i].data[w] >> 16) | (fpi[i].data[w] << 16);
			const uint8_t bytes[4] = {uint8_t(word >> 24), uint8_t(word >> 16), uint8_t(word >> 8), uint8_t(word)};
			out.insert(out.end(), bytes, bytes + 4);
		}
	return true;
}

} // namespace c3dps3

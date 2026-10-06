// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+
//
// Lowers the recompiler IR to ARM64 machine code.
//
// This first iteration keeps every SSA value in a stack slot (no register
// allocation yet) so the generated code is simple and easy to validate
// against the interpreter. Guest state lives in g_cpuRegistersPack and is
// addressed through X19, which the prologue pins and the epilogue restores.
//
// Memory and division use small C helpers with interpreter-exact semantics;
// later iterations replace them with inline VTLB/fastmem emitters.

#pragma once

#include "ir/Ir.h"

#include <string>

namespace EeIr
{
	struct LowerOutput
	{
		u8* entry = nullptr; // callable host function
		u32 host_size = 0;
	};

	// `code` must point at an executable buffer of `capacity` bytes that is not
	// in use. The function pointer in `out` remains valid until the buffer is
	// reused or the instruction cache is invalidated externally.
	bool LowerBlock(ir::Function& fn, u8* code, size_t capacity, LowerOutput* out, std::string* error);
} // namespace EeIr

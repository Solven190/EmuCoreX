// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+
//
// R5900 (EE) integer front end for the shared recompiler IR.
//
// The lifter translates one guest basic block into an ir::Function. Blocks are
// single-entry/single-exit: guest state is read and written explicitly, and
// every exit from the compiled unit is a control terminator (Resume for a
// guest address, BranchIndirect for register targets, Trap for exceptions).
//
// Instructions outside the modelled integer subset make LiftBlock fail, so the
// caller can keep using the legacy recompiler for those blocks.

#pragma once

#include "ir/Ir.h"

#include <string>

namespace EeIr
{
	struct LiftOptions
	{
		u32 start_pc = 0;
		u32 max_insts = 256; // decode cap for a single block
	};

	// `code` points at the host-mapped guest instruction stream for
	// options.start_pc. The lifter never reads across a 4 KiB guest page.
	bool LiftBlock(const u32* code, const LiftOptions& options, ir::Function& out, u32* end_pc,
		std::string* error);

#if defined(EMUCOREX_ENABLE_NATIVE_SELF_TESTS)
	bool RunSelfTests();
#endif
} // namespace EeIr

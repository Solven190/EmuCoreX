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

// Test aid: standalone lowering records the exit target of Resume /
// BranchIndirect terminators here so a driver can follow guest control flow.
// g_eeir_exit_valid distinguishes "no terminator ran" from a target of 0.
extern "C" u32 g_eeir_exit_pc;
extern "C" u32 g_eeir_exit_valid;

namespace EeIr
{
	// Inline-mode exits are emitted through these hooks so the integration can
	// reuse the legacy block-tail machinery (pc store, event test, linking).
	struct LowerHooks
	{
		void (*guest_exit)(void* ctx, u32 guest_pc, bool annulled_delay_slot) = nullptr;
		void (*indirect_exit)(void* ctx) = nullptr; // address arrives in W16
		// Called before every guest memory access so the integration can store
		// the architectural pc (the faulting instruction address) and the
		// delay-slot marker for exception accuracy.
		void (*before_memory)(void* ctx, u32 guest_pc, bool delay_slot) = nullptr;
		// Called around every C helper call so the integration can keep the
		// cycle delta (W24) coherent with cpuRegs.cycle/nextEventCycle.
		void (*before_helper)(void* ctx) = nullptr;
		void (*after_helper)(void* ctx) = nullptr;
		void* ctx = nullptr;
	};

	struct LowerOptions
	{
		// Inline body: uses a temporary stack frame, without code-buffer management. The
		// guest base is the pinned X27, the frame base is X20, and every exit
		// is emitted through the hooks above. Only allowed in this mode:
		// Jump, Branch, BranchIndirect and Resume.
		bool inline_body = false;
		const LowerHooks* hooks = nullptr;
		bool capture_exit_pc = false; // standalone: record exit targets
	};

	struct LowerOutput
	{
		u8* entry = nullptr; // callable host function (standalone mode only)
		u32 host_size = 0;
	};

	// `code` must point at an executable buffer of `capacity` bytes that is not
	// in use. The function pointer in `out` remains valid until the buffer is
	// reused or the instruction cache is invalidated externally.
	bool LowerBlock(ir::Function& fn, const LowerOptions& options, u8* code, size_t capacity,
		LowerOutput* out, std::string* error);

	bool LowerBlock(ir::Function& fn, u8* code, size_t capacity, LowerOutput* out, std::string* error);

	// Validation-only pass: true when LowerBlock is guaranteed to accept the
	// function. Used by the integration path to decide before emitting code.
	bool CanLower(const ir::Function& fn, bool inline_body, std::string* error);
} // namespace EeIr

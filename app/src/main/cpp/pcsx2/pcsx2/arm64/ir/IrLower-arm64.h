// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+
//
// Lowers the recompiler IR to ARM64 machine code.
//
// A conservative linear scan assigns integer SSA values to call-preserved
// host registers, spilling values to the stack when register pressure requires.
// Full 128-bit SIMD values use caller-save registers, with typed stack homes
// and save/restore around memory hooks and C helpers when their values are live.
// Integer constants are materialized at their uses, reducing spills and
// stack frame sizes while keeping the generated code easy to validate
// against the interpreter. Guest state lives in g_cpuRegistersPack and is
// addressed through pinned X27 in inline mode, or X19 in standalone mode
// where the prologue pins it and the epilogue restores it.
//
// Memory checks live mappings for direct reads and uses small C helpers for
// handlers and cache emulation with interpreter-exact semantics. Optimized
// 32-bit division/remainder use ARM64 with R5900 edge-case corrections; the
// unoptimized path retains helpers for differential comparison.

#pragma once

#include "ir/Ir.h"

#include <string>

// Test aid: standalone lowering records the exit target of Resume /
// BranchIndirect terminators here so a driver can follow guest control flow.
// g_eeir_exit_valid distinguishes "no terminator ran" from a target of 0.
extern "C" u32 g_eeir_exit_pc;
extern "C" u32 g_eeir_exit_valid;

namespace Arm64Ir
{
	enum class GuestState : u8 { EE, IOP };

	// Narrow inline VU slice. microVU owns the VI mappings and backup/dirty
	// bookkeeping; lhs_dst is already initialized from lhs_vi. No frame or
	// guest-memory access is emitted, and IR Return emits no machine RET.
	// Sub with equal VI operands folds to zero and requires no input binding.
	struct BoundViPlan
	{
		ir::Op alu = ir::Op::Nop;
		u8 lhs_vi = 0;
		u8 rhs_vi = 0;
		u8 dst_vi = 0;
		bool immediate = false;
		u32 constant = 0;
	};
	bool PlanBoundViBlock(const ir::Function& fn, BoundViPlan* plan, std::string* error);

	// Narrow cross-domain VU transfer. The caller owns both bindings; vf_source
	// contains the selected VF lane in S0, as produced by microVU's lane-aware
	// allocRegId(Fs, 0, component_mask) path.
	struct BoundVfViTransferPlan
	{
		u8 vf = 0;
		u8 lane = 0;
		u8 dst_vi = 0;
		bool delay_slot = false;
	};
	struct LowerOutput;
	bool PlanBoundVfViTransfer(const ir::Function& fn, BoundVfViTransferPlan* plan, std::string* error);
	bool LowerBoundVfViTransfer(const ir::Function& fn, u32 vf_source, u32 vi_dst,
		LowerOutput* out, std::string* error);
	bool LowerBoundVfViTransferPlan(const BoundVfViTransferPlan& plan, u32 vf_source, u32 vi_dst,
		LowerOutput* out, std::string* error);

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
		// Runs after the access on both direct-memory and helper paths.
		void (*after_memory)(void* ctx) = nullptr;
		// Called around every C helper call so the integration can keep the
		// cycle delta (W24) coherent with cpuRegs.cycle/nextEventCycle.
		void (*before_helper)(void* ctx) = nullptr;
		void (*after_helper)(void* ctx) = nullptr;
		void* ctx = nullptr;
		// Explicit contract for before_memory/after_memory only. C helpers and
		// their surrounding hooks always clobber caller-save SIMD registers.
		bool memory_preserves_vectors = false;
	};

	struct LowerOptions
	{

		// Inline body: only spills need a temporary stack frame; no code-buffer
		// management. The guest base is the pinned X27, the frame base is X20, and every exit
		// is emitted through the hooks above. Only allowed in this mode:
		// Jump, Branch, BranchIndirect and Resume.
		bool inline_body = false;
		const LowerHooks* hooks = nullptr;
		bool capture_exit_pc = false; // standalone: record exit targets
		// Integer constants are materialized at their uses and need no spill
		// slot. The oracle disables this to compare both lowering paths.
		bool materialize_constants = true;
		// Conservative linear-scan allocation in call-preserved host registers.
		// Both modes remain available to the interpreter differential oracle.
		bool allocate_registers = true;
		// Allocate full-width SIMD values, preserving live ones around helpers.
		bool allocate_vector_registers = true;
		// Preserve state writes and memory/helper barriers while forwarding
		// redundant reads, folding integer constants, removing dead values and
		// inlining 32-bit division/remainder with guest-exact edge semantics.
		bool optimize_ir = true;
		// Keep helper-based division available for isolated runtime A/B tests.
		bool inline_division = true;
		// Check the live VTLB entry and access mapped memory directly; handler
		// entries retain the interpreter helper. Disabled for EE cache emulation.
		bool direct_quad_memory = true;
		// Direct mapped EE 8/16/32/64-bit reads; retain helpers for handlers/cache.
		bool direct_ee_loads = true;
		// Keep SIMD values in registers on direct RAM paths; spill on fallback.
		bool direct_quad_vectors = true;
		// Reuse typed spill slots after their SSA values die within a block.
		bool reuse_spill_slots = true;
		// Keep a separate block-local cache for full 128-bit GPR values.
		bool forward_quad_state = true;
		GuestState guest_state = GuestState::EE;
		bool direct_iop_loads = true; // live RAM LUT; handlers for other IOP reads
	};

	struct LowerOutput
	{
		u8* entry = nullptr; // callable host function (standalone mode only)
		u32 host_size = 0;
		u32 frame_size = 0;
		u32 register_values = 0;
		u32 spill_values = 0;
		u32 spill_slots = 0;
		u32 direct_quad_operations = 0;
		u32 direct_ee_load_operations = 0;
		u32 direct_quad_vector_operations = 0;
		u32 vector_register_values = 0;
		u32 vector_save_values = 0; // allocated SIMD values needing stack homes
		u32 iop_memory_operations = 0;
		u32 direct_iop_load_operations = 0;
		u32 fused_multiply_pairs = 0;
	};

	// Both host W-register indices must be in [0,30], owned by the caller.
	// Rejects an unsupported function before changing the active Oaknut buffer.
	bool LowerBoundViBlock(const ir::Function& fn, u32 lhs_dst, u32 rhs,
		LowerOutput* out, std::string* error);
	// Consume a plan produced by PlanBoundViBlock once before guest bindings.
	// Immediate plans may use rhs as caller-owned constant scratch. No scratch
	// is needed for VI0 constants or a representable Add/Sub immediate.
	bool LowerBoundViPlan(const BoundViPlan& plan, u32 lhs_dst, u32 rhs,
		LowerOutput* out, std::string* error);

	// `code` must point at an executable buffer of `capacity` bytes that is not
	// in use. The function pointer in `out` remains valid until the buffer is
	// reused or the instruction cache is invalidated externally.
	bool LowerBlock(ir::Function& fn, const LowerOptions& options, u8* code, size_t capacity,
		LowerOutput* out, std::string* error);

	bool LowerBlock(ir::Function& fn, u8* code, size_t capacity, LowerOutput* out, std::string* error);

	// Validation-only pass: true when LowerBlock is guaranteed to accept the
	// function. Used by the integration path to decide before emitting code.
	bool CanLower(const ir::Function& fn, bool inline_body, std::string* error, GuestState guest_state = GuestState::EE);
} // namespace Arm64Ir

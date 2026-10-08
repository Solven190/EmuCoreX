// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+

#include "arm64/ir/IrLower-arm64.h"

#include "arm64/OaknutHelpers-arm64.h"
#include "arm64/cpuRegistersPack-arm64.h"
#include "Memory.h"
#include "IopMem.h"
#include "IopDma.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <optional>
#include <vector>

namespace Arm64Ir
{
	extern "C" u32 g_eeir_exit_pc = 0;
	extern "C" u32 g_eeir_exit_valid = 0;

	namespace
	{
		// C++ helpers referenced by the lowered code. ABI: integer arguments in
		// W0/W1, results in W0 (or X0 for 64-bit loads).
		extern "C"
		{
			u32 EeIrMemRead8(u32 addr) { return memRead8(addr); }
			u32 EeIrMemRead8S(u32 addr) { return static_cast<u32>(static_cast<s32>(static_cast<s8>(memRead8(addr)))); }
			u32 EeIrMemRead16(u32 addr) { return memRead16(addr); }
			u32 EeIrMemRead16S(u32 addr) { return static_cast<u32>(static_cast<s32>(static_cast<s16>(memRead16(addr)))); }
			u32 EeIrMemRead32(u32 addr) { return memRead32(addr); }
			u64 EeIrMemRead64(u32 addr) { return memRead64(addr); }
			void EeIrMemWrite8(u32 addr, u32 value) { memWrite8(addr, static_cast<u8>(value)); }
			void EeIrMemWrite16(u32 addr, u32 value) { memWrite16(addr, static_cast<u16>(value)); }
			void EeIrMemWrite32(u32 addr, u32 value) { memWrite32(addr, value); }
			void EeIrMemWrite64(u32 addr, u64 value) { memWrite64(addr, value); }
			void EeIrMemRead128(u32 addr, mem128_t* out) { memRead128(addr, out); }
			void EeIrMemWrite128(u32 addr, const mem128_t* value) { memWrite128(addr, value); }

			u32 IopIrMemRead8(u32 addr) { return iopMemRead8(addr); }
			u32 IopIrMemRead8S(u32 addr) { return static_cast<u32>(static_cast<s32>(static_cast<s8>(iopMemRead8(addr)))); }
			u32 IopIrMemRead16(u32 addr) { return iopMemRead16(addr); }
			u32 IopIrMemRead16S(u32 addr) { return static_cast<u32>(static_cast<s32>(static_cast<s16>(iopMemRead16(addr)))); }
			u32 IopIrMemRead32(u32 addr) { return iopMemRead32(addr); }
			void IopIrMemWrite8(u32 addr, u32 value) { iopMemWrite8(addr, static_cast<u8>(value)); }
			void IopIrMemWrite16(u32 addr, u32 value) { iopMemWrite16(addr, static_cast<u16>(value)); }
			void IopIrMemWrite32(u32 addr, u32 value) { iopMemWrite32(addr, value); }

			// DIV/DIVU edge cases match R5900OpcodeImpl exactly.
			u32 EeIrDivSLo(u32 a, u32 b)
			{
				if (a == 0x80000000u && b == 0xffffffffu)
					return 0x80000000u;
				if (b != 0)
					return static_cast<u32>(static_cast<s32>(a) / static_cast<s32>(b));
				return (static_cast<s32>(a) < 0) ? 1u : 0xffffffffu;
			}

			u32 EeIrDivSHi(u32 a, u32 b)
			{
				if (a == 0x80000000u && b == 0xffffffffu)
					return 0;
				if (b != 0)
					return static_cast<u32>(static_cast<s32>(a) % static_cast<s32>(b));
				return a;
			}

			u32 EeIrDivULo(u32 a, u32 b) { return (b != 0) ? a / b : 0xffffffffu; }
			u32 EeIrDivUHi(u32 a, u32 b) { return (b != 0) ? a % b : a; }
		}

		bool IsSupportedOp(ir::Op op)
		{
			switch (op)
			{
				case ir::Op::Nop:
				case ir::Op::ConstI32:
				case ir::Op::ConstF32:
				case ir::Op::ConstI64:
				case ir::Op::ConstVec:
				case ir::Op::Copy:
				case ir::Op::Undef:
				case ir::Op::Select:
				case ir::Op::Not:
				case ir::Op::Neg:
				case ir::Op::Sext8:
				case ir::Op::Sext16:
				case ir::Op::Zext8:
				case ir::Op::Zext16:
				case ir::Op::Sext32:
				case ir::Op::Zext32:
				case ir::Op::Trunc32:
				case ir::Op::Add:
				case ir::Op::Sub:
				case ir::Op::And:
				case ir::Op::Or:
				case ir::Op::Xor:
				case ir::Op::MinS:
				case ir::Op::MinU:
				case ir::Op::MaxS:
				case ir::Op::MaxU:
				case ir::Op::Mul:
				case ir::Op::MulHiS:
				case ir::Op::MulHiU:
				case ir::Op::Msub:
				case ir::Op::Shl:
				case ir::Op::ShrU:
				case ir::Op::ShrS:
				case ir::Op::VShl:
				case ir::Op::VShuffle:
				case ir::Op::VShuffle2:
				case ir::Op::VShrU:
				case ir::Op::VShrS:
				case ir::Op::VMinS:
				case ir::Op::VMinU:
				case ir::Op::VMaxS:
				case ir::Op::VMaxU:
				case ir::Op::VCmpEq:
				case ir::Op::VCmpNe:
				case ir::Op::VCmpLtS:
				case ir::Op::VCmpLtU:
				case ir::Op::VCmpLeS:
				case ir::Op::VCmpLeU:
				case ir::Op::CmpEq:
				case ir::Op::CmpNe:
				case ir::Op::CmpLtS:
				case ir::Op::CmpLtU:
				case ir::Op::CmpLeS:
				case ir::Op::CmpLeU:
				case ir::Op::CmpGtS:
				case ir::Op::CmpGtU:
				case ir::Op::CmpGeS:
				case ir::Op::CmpGeU:
				case ir::Op::DivS:
				case ir::Op::RemS:
				case ir::Op::DivU:
				case ir::Op::RemU:
				case ir::Op::Load8U:
				case ir::Op::Load8S:
				case ir::Op::Load16U:
				case ir::Op::Load16S:
				case ir::Op::Load32:
				case ir::Op::Load64:
				case ir::Op::Load128:
				case ir::Op::Store8:
				case ir::Op::Store16:
				case ir::Op::Store32:
				case ir::Op::Store64:
				case ir::Op::Store128:
				case ir::Op::ReadCp0:
				case ir::Op::WriteCp0:
				case ir::Op::CheckInterrupts:
				case ir::Op::ReadGpr:
				case ir::Op::WriteGpr:
				case ir::Op::ReadHi:
				case ir::Op::ReadLo:
				case ir::Op::WriteHi:
				case ir::Op::WriteLo:
				case ir::Op::AddCycles:
					return true;
				default:
					return false;
			}
		}


		// Forward reads only within one basic block. Keep every architectural
		// write in place: a following memory helper may observe state or fault.
		// Helpers/accesses are barriers even for ordinary RAM because the same
		// operation can dispatch MMIO, events or guest exception handlers.
		void ForwardGuestStateReads(ir::Function& fn, bool inline_division, bool forward_quad_state)
		{
			struct CachedState { u32 narrow = 0, wide = 0; bool sign_extended = false; u32 quad = 0; };
			for (ir::Block& block : fn.blocks)
			{
				std::array<CachedState, 66> cache{}; // GPR[32], HI, LO, CP0[32]
				for (ir::Inst& inst : block.insts)
				{
					const auto kind = ir::Info(inst.op).kind;
					if (kind == ir::OpKind::MemLoad || kind == ir::OpKind::MemStore || kind == ir::OpKind::Helper ||
						(!inline_division && (inst.op == ir::Op::DivS || inst.op == ir::Op::DivU ||
							inst.op == ir::Op::RemS || inst.op == ir::Op::RemU)))
					{
						cache = {};
						continue;
					}
					const bool read = inst.op == ir::Op::ReadGpr || inst.op == ir::Op::ReadHi || inst.op == ir::Op::ReadLo || inst.op == ir::Op::ReadCp0;
					const bool write = inst.op == ir::Op::WriteGpr || inst.op == ir::Op::WriteHi || inst.op == ir::Op::WriteLo || inst.op == ir::Op::WriteCp0;
					if (!read && !write)
						continue;
					const u32 reg = (inst.op == ir::Op::ReadCp0 || inst.op == ir::Op::WriteCp0) ? 34u + static_cast<u32>(inst.imm) :
						(inst.op == ir::Op::ReadGpr || inst.op == ir::Op::WriteGpr) ?
						static_cast<u32>(inst.imm) : (inst.op == ir::Op::ReadHi || inst.op == ir::Op::WriteHi) ? 32u : 33u;
					if (reg == 0)
						continue;
					CachedState& state = cache[reg];
					const ir::Type type = read ? inst.type : fn.ValueType(inst.args[0]);
					if (forward_quad_state && reg < 32 && type == ir::Type::V4U32)
					{
						if (write)
						{
							// A full write invalidates cached scalar views of this GPR.
							state = {};
							state.quad = inst.args[0];
						}
						else
						{
							if (state.quad)
							{
								inst.op = ir::Op::Copy;
								inst.args[0] = state.quad;
								inst.num_args = 1;
								inst.imm = 0;
							}
							state.quad = inst.value;
						}
						continue;
					}
					if (!ir::IsIntegerType(type))
					{
						state = {};
						continue;
					}
					if (write)
					{
						const bool wide = reg < 32 ? (inst.aux & ir::IF_WIDE_WRITE) != 0 : fn.ValueType(inst.args[0]) == ir::Type::I64;
						// A partial write changes the low half, so the old full value
						// is unusable even though the architectural upper half survives.
						state = wide ? CachedState{0, inst.args[0], false} : CachedState{inst.args[0], 0, true};
						continue;
					}
					const bool wide = inst.type == ir::Type::I64;
					const u32 source = wide ? state.wide : state.narrow;
					if (source)
					{
						inst.op = ir::Op::Copy;
						inst.args[0] = source;
						inst.num_args = 1;
					}
					else if ((!wide && state.wide) || (wide && state.narrow && state.sign_extended))
					{
						inst.op = wide ? ir::Op::Sext32 : ir::Op::Trunc32;
						inst.args[0] = wide ? state.narrow : state.wide;
						inst.num_args = 1;
					}
					if (wide)
						state.wide = inst.value;
					else
						state.narrow = inst.value;
					// A narrow READ alone says nothing about bits 32..63. Only a
					// narrow architectural WRITE establishes the sign extension.
					if (inst.op == ir::Op::Copy || inst.op == ir::Op::Sext32 || inst.op == ir::Op::Trunc32)
						inst.imm = 0;
				}
			}
		}

		class Lowerer
		{
		public:
			Lowerer(ir::Function& fn, const LowerOptions& options)
				: m_fn(fn)
				, m_guest_state(options.guest_state)
				, m_inline(options.inline_body)
				, m_hooks(options.hooks)
				, m_capture_exit(options.capture_exit_pc)
				, m_materialize_constants(options.materialize_constants)
				, m_allocate_registers(options.allocate_registers)
				, m_allocate_vectors(options.allocate_registers && options.allocate_vector_registers)
				, m_inline_division(options.optimize_ir && options.inline_division)
				, m_fuse_multiply_pairs(options.optimize_ir)
				, m_direct_quad_memory(options.optimize_ir && options.direct_quad_memory && (!CHECK_CACHE || CHECK_EEREC))
				, m_direct_quad_vectors(options.direct_quad_vectors)
				, m_direct_iop_loads(options.optimize_ir && options.direct_iop_loads)
				, m_reuse_spill_slots(options.optimize_ir && options.reuse_spill_slots)
			{
			}

			bool Run(u8* code, size_t capacity, LowerOutput* out, std::string* error);

			oak::XReg FrameBase() const { return m_inline ? oak::util::X20 : oak::util::X28; }
			oak::XReg GuestBase() const { return m_inline ? oak::util::X27 : oak::util::X19; }

		private:
			u32 SaveArea() const { return m_inline ? 0u : (m_allocate_registers ? 64u : 32u); }
			static constexpr int kUnallocated = -1;
			void AllocateRegisters();
			void AllocateVectorRegisters();
			bool IsVectorBarrier(const ir::Inst& inst) const;
			bool UsesDirectVectorMemory(const ir::Inst& inst) const
			{
				return m_allocate_vectors && m_direct_quad_memory && m_direct_quad_vectors &&
					(!m_hooks || m_hooks->memory_preserves_vectors) &&
					(inst.op == ir::Op::Load128 || inst.op == ir::Op::Store128);
			}
			void SaveVectors(u32 position)
			{
				for (u32 value : m_vector_saves[position])
					oakStore128(oak::QReg(m_vector_registers[value]), {FrameBase(), static_cast<s64>(Slot(value))});
			}
			void RestoreVectors(u32 position)
			{
				for (u32 value : m_vector_restores[position])
					oakLoad128(oak::QReg(m_vector_registers[value]), {FrameBase(), static_cast<s64>(Slot(value))});
			}
			oak::QReg Result128(u32 value) const
			{
				return oak::QReg(!m_at_vector_barrier && m_vector_registers[value] >= 0 ? m_vector_registers[value] : 0);
			}
			oak::QReg Operand128(u32 value, const oak::QReg& scratch)
			{
				if (!m_at_vector_barrier && m_vector_registers[value] >= 0)
					return oak::QReg(m_vector_registers[value]);
				Load128(value, scratch);
				return scratch;
			}
			oak::WReg Result32(u32 value) const { return oak::WReg(m_registers[value] >= 0 ? m_registers[value] : 0); }
			oak::XReg Result64(u32 value) const { return oak::XReg(m_registers[value] >= 0 ? m_registers[value] : 0); }
			oak::WReg Operand32(u32 value, const oak::WReg& scratch)
			{
				if (m_registers[value] >= 0)
					return oak::WReg(m_registers[value]);
				Load32(value, scratch);
				return scratch;
			}
			oak::XReg Operand64(u32 value, const oak::XReg& scratch)
			{
				if (m_registers[value] >= 0)
					return oak::XReg(m_registers[value]);
				Load64(value, scratch);
				return scratch;
			}

			template <typename Reg>
			bool EmitAddSubImmediate(const ir::Inst& inst, const Reg& dst, const Reg& src, u64 constant)
			{
				if (inst.op != ir::Op::Add && inst.op != ir::Op::Sub)
					return false;
				const u64 mask = inst.type == ir::Type::I64 ? ~u64(0) : 0xffffffffull;
				constant &= mask;
				bool subtract = inst.op == ir::Op::Sub;
				if (constant > 4095)
				{
					constant = (u64(0) - constant) & mask;
					subtract = !subtract;
				}
				if (constant > 4095)
					return false;
				if (subtract)
					oakAsm->SUB(dst, src, constant);
				else
					oakAsm->ADD(dst, src, constant);
				return true;
			}

			u32 Slot(u32 value) const { return m_slots[value]; }

			void Load32(u32 value, const oak::WReg& dst)
			{
				if (m_registers[value] >= 0)
				{
					const oak::WReg src(m_registers[value]);
					// MOV Wn,Wn must execute when truncating a coalesced I64.
					if (dst.index() != src.index() || m_fn.ValueType(value) == ir::Type::I64)
						oakAsm->MOV(dst, src);
				}
				else if (m_constants[value])
					oakAsm->MOV(dst, static_cast<u32>(*m_constants[value]));
				else
					oakLoad32(dst, {FrameBase(), static_cast<s64>(Slot(value))});
			}

			void Load64(u32 value, const oak::XReg& dst)
			{
				if (m_registers[value] >= 0)
				{
					const oak::XReg src(m_registers[value]);
					if (dst.index() != src.index())
						oakAsm->MOV(dst, src);
				}
				else if (m_constants[value])
					oakAsm->MOV(dst, *m_constants[value]);
				else
					oakLoad64(dst, {FrameBase(), static_cast<s64>(Slot(value))});
			}

			void Store32(u32 value, const oak::WReg& src)
			{
				if (m_registers[value] >= 0)
				{
					const oak::WReg dst(m_registers[value]);
					if (dst.index() != src.index())
						oakAsm->MOV(dst, src);
				}
				else
					oakStore32(src, {FrameBase(), static_cast<s64>(Slot(value))});
			}

			void Store64(u32 value, const oak::XReg& src)
			{
				if (m_registers[value] >= 0)
				{
					const oak::XReg dst(m_registers[value]);
					if (dst.index() != src.index())
						oakAsm->MOV(dst, src);
				}
				else
					oakStore64(src, {FrameBase(), static_cast<s64>(Slot(value))});
			}

			void Load128(u32 value, const oak::QReg& dst)
			{
				if (!m_at_vector_barrier && m_vector_registers[value] >= 0)
				{
					const oak::QReg src(m_vector_registers[value]);
					if (dst.index() != src.index())
						oakAsm->MOV(dst.B16(), src.B16());
				}
				else
					oakLoad128(dst, {FrameBase(), static_cast<s64>(Slot(value))});
			}

			void Store128(u32 value, const oak::QReg& src)
			{
				if (!m_at_vector_barrier && m_vector_registers[value] >= 0)
				{
					const oak::QReg dst(m_vector_registers[value]);
					if (dst.index() != src.index())
						oakAsm->MOV(dst.B16(), src.B16());
				}
				else
					oakStore128(src, {FrameBase(), static_cast<s64>(Slot(value))});
			}

			void SlotAddress(u32 value, const oak::XReg& dst)
			{
				const u32 offset = Slot(value);
				if (offset <= 4095)
					oakAsm->ADD(dst, FrameBase(), offset);
				else
				{
					oakAsm->ADD(dst, FrameBase(), offset >> 12, oak::LslSymbol::LSL, 12);
					if (offset & 0xfff)
						oakAsm->ADD(dst, dst, offset & 0xfff);
				}
			}

			s64 GprOffset(u32 reg) const
			{
				if (m_guest_state == GuestState::IOP)
					return static_cast<s64>(offsetof(cpuRegistersPack, psxRegs.GPR.r[0])) + reg * sizeof(u32);
				return static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0])) +
					static_cast<s64>(reg) * static_cast<s64>(sizeof(GPR_reg));
			}

			s64 LoOffset() const { return m_guest_state == GuestState::IOP ? offsetof(cpuRegistersPack, psxRegs.GPR.n.lo) : offsetof(cpuRegistersPack, cpuRegs.LO); }
			s64 HiOffset() const { return m_guest_state == GuestState::IOP ? offsetof(cpuRegistersPack, psxRegs.GPR.n.hi) : offsetof(cpuRegistersPack, cpuRegs.HI); }
			s64 PcOffset() const { return m_guest_state == GuestState::IOP ? offsetof(cpuRegistersPack, psxRegs.pc) : offsetof(cpuRegistersPack, cpuRegs.pc); }

			void EmitFrameAdjust(bool add, u32 amount);
			void EmitPrologue();
			void EmitEntryJump();
			void EmitEpilogue();
			bool EmitBlock(const ir::Block& block, std::string* error);
			bool TryEmitMultiplyPair(const ir::Inst& lo, const ir::Inst& hi);
			bool EmitInst(const ir::Inst& inst, std::string* error);
			bool EmitInstBody(const ir::Inst& inst, std::string* error);
			bool Fail(std::string* error, const char* what);

			ir::Function& m_fn;
			GuestState m_guest_state;
			std::vector<oak::Label> m_labels;
			u32 m_frame = 0;
			bool m_inline = false;
			const LowerHooks* m_hooks = nullptr;
			bool m_capture_exit = false;
			bool m_materialize_constants = true;
			bool m_allocate_registers = true;
			bool m_allocate_vectors = true;
			bool m_at_vector_barrier = false;
			u32 m_emit_position = 0;
			bool m_inline_division = true;
			bool m_fuse_multiply_pairs = true;
			u32 m_fused_multiply_pairs = 0;
			bool m_direct_quad_memory = true;
			bool m_direct_quad_vectors = true;
			bool m_direct_iop_loads = true;
			bool m_reuse_spill_slots = true;
			u32 m_iop_memory_operations = 0;
			u32 m_direct_iop_load_operations = 0;
			u32 m_direct_quad_operations = 0;
			u32 m_direct_quad_vector_operations = 0;
			std::vector<std::optional<u64>> m_constants;
			std::vector<u32> m_slots;
			std::vector<int> m_registers;
			std::vector<int> m_vector_registers;
			std::vector<bool> m_vector_homes;
			std::vector<std::vector<u32>> m_vector_saves, m_vector_restores;
		};

		void Lowerer::AllocateRegisters()
		{
			m_registers.assign(m_fn.value_types.size(), kUnallocated);
			if (!m_allocate_registers)
				return;
			struct Interval { u32 value, first, last; };
			std::vector<Interval> intervals;
			std::vector<u32> last_use(m_fn.value_types.size(), 0);
			u32 position = 0;
			for (const ir::Block& block : m_fn.blocks)
			{
				for (const ir::Inst& inst : block.insts)
				{
					const u32 operands = ir::ValueOperandCount(inst);
					for (u32 i = 0; i < operands; ++i)
						last_use[inst.args[i]] = position;
					if (inst.value && !m_constants[inst.value] && ir::IsIntegerType(inst.type))
						intervals.push_back({inst.value, position, position});
					++position;
				}
			}
			// X20 is the inline frame base, X24 the cycle delta, X25 the old
			// fastmem base, X26 the VU clamp base, and X27..X29 dispatcher
			// bases. These three registers
			// are available within an EE block and survive AAPCS64 C helpers.
			constexpr int host_regs[] = {21, 22, 23};
			std::vector<Interval> active;
			for (Interval interval : intervals)
			{
				interval.last = std::max(interval.first, last_use[interval.value]);
				active.erase(std::remove_if(active.begin(), active.end(), [&](const Interval& live) {
					// Every emitter reads all operands before defining its result,
				// so a register can be reused at the operand's final use.
				return live.last <= interval.first;
				}), active.end());
				for (int reg : host_regs)
				{
					const bool busy = std::any_of(active.begin(), active.end(), [&](const Interval& live) {
						return m_registers[live.value] == reg;
					});
					if (!busy)
					{
						m_registers[interval.value] = reg;
						active.push_back(interval);
						break;
					}
				}
			}
		}

		bool Lowerer::IsVectorBarrier(const ir::Inst& inst) const
		{
			const auto kind = ir::Info(inst.op).kind;
			return kind == ir::OpKind::MemLoad || kind == ir::OpKind::MemStore || kind == ir::OpKind::Helper ||
				(!m_inline_division && (inst.op == ir::Op::DivS || inst.op == ir::Op::DivU ||
					inst.op == ir::Op::RemS || inst.op == ir::Op::RemU));
		}

		void Lowerer::AllocateVectorRegisters()
		{
			const u32 count = static_cast<u32>(m_fn.value_types.size());
			m_vector_registers.assign(count, kUnallocated);
			m_vector_homes.assign(count, false);
			if (!m_allocate_vectors)
				return;
			struct Interval { u32 value, first, last; };
			std::vector<Interval> intervals, active;
			std::vector<u32> last_use(count), barriers;
			u32 position = 0;
			for (const ir::Block& block : m_fn.blocks)
				for (const ir::Inst& inst : block.insts)
				{
					for (u32 arg = 0; arg < ir::ValueOperandCount(inst); ++arg)
						last_use[inst.args[arg]] = position;
					if (inst.value && inst.type == ir::Type::V4U32)
						intervals.push_back({inst.value, position, position});
					if (IsVectorBarrier(inst))
						barriers.push_back(position);
					++position;
				}
			m_vector_saves.resize(position);
			m_vector_restores.resize(position);
			// Q0/Q1 are emitter temporaries. Q2..Q7 are caller-save in full;
			// AAPCS64 preserves only the low 64 bits of V8..V15, not full quads.
			constexpr int host_regs[] = {2, 3, 4, 5, 6, 7};
			for (Interval interval : intervals)
			{
				interval.last = std::max(interval.first, last_use[interval.value]);
				active.erase(std::remove_if(active.begin(), active.end(), [&](const Interval& live) {
					return live.last <= interval.first;
				}), active.end());
				for (int reg : host_regs)
				{
					if (std::any_of(active.begin(), active.end(), [&](const Interval& live) {
						return m_vector_registers[live.value] == reg;
					}))
						continue;
					m_vector_registers[interval.value] = reg;
					active.push_back(interval);
					// Homes are needed only at barriers: live inputs are saved
					// before hooks, and live outputs restored after all hooks.
					// Inclusive endpoints cover Store128's pointer argument and
					// Load128's helper result, even at the final/first use.
					for (auto it = std::lower_bound(barriers.begin(), barriers.end(), interval.first);
						it != barriers.end() && *it <= interval.last; ++it)
					{
						m_vector_homes[interval.value] = true;
						if (interval.first < *it)
							m_vector_saves[*it].push_back(interval.value);
						if (*it < interval.last)
							m_vector_restores[*it].push_back(interval.value);
					}
					break;
				}
			}
		}

		bool Lowerer::Fail(std::string* error, const char* what)
		{
			if (error)
			{
				char buffer[96];
				std::snprintf(buffer, sizeof(buffer), "IR lowering: %s", what);
				*error = buffer;
			}
			return false;
		}

		void Lowerer::EmitFrameAdjust(bool add, u32 amount)
		{
			if (amount == 0)
				return;
			// AArch64 ADD/SUB immediates encode 0..4095 or an imm12 shifted by
			// 12, so larger frames are adjusted with two instructions.
			if (amount <= 4095)
			{
				if (add)
					oakAsm->ADD(oak::util::SP, oak::util::SP, amount);
				else
					oakAsm->SUB(oak::util::SP, oak::util::SP, amount);
				return;
			}

			const u32 hi = amount >> 12;
			const u32 lo = amount & 0xfffu;
			if (add)
			{
				oakAsm->ADD(oak::util::SP, oak::util::SP, lo);
				oakAsm->ADD(oak::util::SP, oak::util::SP, hi, oak::LslSymbol::LSL, 12);
			}
			else
			{
				oakAsm->SUB(oak::util::SP, oak::util::SP, hi, oak::LslSymbol::LSL, 12);
				oakAsm->SUB(oak::util::SP, oak::util::SP, lo);
			}
		}

		void Lowerer::EmitPrologue()
		{
			// Inline blocks use the dispatcher's saved registers; only actual
			// spills need a frame. Spill-free blocks need no SP/frame-base work.
			if (m_inline && m_frame == 0)
				return;
			recBeginOaknutEmit();
			EmitFrameAdjust(false, m_frame);
			if (m_inline)
			{
				oakAsm->MOV(oak::util::X20, oak::util::SP);
			}
			else
			{
				oakAsm->STP(oak::util::X19, oak::util::X30, oak::util::SP, oak::SOffset<10, 3>(0));
				oakAsm->STP(oak::util::X28, oak::util::XZR, oak::util::SP, oak::SOffset<10, 3>(16));
				if (m_allocate_registers)
				{
					oakAsm->STP(oak::util::X21, oak::util::X22, oak::util::SP, oak::SOffset<10, 3>(32));
					oakAsm->STP(oak::util::X23, oak::util::XZR, oak::util::SP, oak::SOffset<10, 3>(48));
				}
				oakAsm->MOV(oak::util::X28, oak::util::SP);
				oakMoveAddressToReg(oak::util::X19, &g_cpuRegistersPack);
			}
			recEndOaknutEmit();
		}

		void Lowerer::EmitEntryJump()
		{
			// Block ids follow storage order, but the declared entry can differ.
			// The normal lifter uses entry 1 and pays no runtime instruction here.
			if (m_fn.entry == 1)
				return;
			recBeginOaknutEmit();
			oakAsm->B(m_labels[m_fn.entry - 1]);
			recEndOaknutEmit();
		}

		void Lowerer::EmitEpilogue()
		{
			recBeginOaknutEmit();
			if (m_inline)
			{
				EmitFrameAdjust(true, m_frame);
			}
			else
			{
				oakAsm->LDP(oak::util::X28, oak::util::XZR, oak::util::SP, oak::SOffset<10, 3>(16));
				if (m_allocate_registers)
				{
					oakAsm->LDP(oak::util::X21, oak::util::X22, oak::util::SP, oak::SOffset<10, 3>(32));
					oakAsm->LDP(oak::util::X23, oak::util::XZR, oak::util::SP, oak::SOffset<10, 3>(48));
				}
				oakAsm->LDP(oak::util::X19, oak::util::X30, oak::util::SP, oak::SOffset<10, 3>(0));
				EmitFrameAdjust(true, m_frame);
				oakAsm->RET();
			}
			recEndOaknutEmit();
		}

		bool Lowerer::Run(u8* code, size_t capacity, LowerOutput* out, std::string* error)
		{
			if (!CanLower(m_fn, m_inline, error, m_guest_state))
				return false;

			const u32 value_count = static_cast<u32>(m_fn.value_types.size());
			m_constants.resize(value_count);
			m_slots.resize(value_count);
			if (m_materialize_constants)
			{
				for (const ir::Block& block : m_fn.blocks)
				{
					for (const ir::Inst& inst : block.insts)
					{
						if (inst.op == ir::Op::ConstI32 || inst.op == ir::Op::ConstI64)
							m_constants[inst.value] = inst.imm;
					}
				}
			}
			AllocateRegisters();
			AllocateVectorRegisters();
			u32 next_slot = SaveArea();
			struct Spill { u32 last, size, slot; };
			std::array<std::vector<u32>, 2> free_slots;
			for (const ir::Block& block : m_fn.blocks)
			{
				std::vector<u32> last_use(value_count);
				u32 position = 0;
				for (const ir::Inst& inst : block.insts)
				{
					for (u32 arg = 0; arg < ir::ValueOperandCount(inst); ++arg)
						last_use[inst.args[arg]] = position;
					++position;
				}
				std::vector<Spill> active;
				position = 0;
				for (const ir::Inst& inst : block.insts)
				{
					if (m_reuse_spill_slots)
					{
						active.erase(std::remove_if(active.begin(), active.end(), [&](const Spill& spill) {
							// Emitters read all operands before writing the result,
							// including addresses passed to memory helpers.
							if (spill.last > position)
								return false;
							free_slots[spill.size == 16].push_back(spill.slot);
							return true;
						}), active.end());
					}
					const u32 value = inst.value;
					const bool allocated = value && (m_registers[value] >= 0 || m_vector_registers[value] >= 0);
					if (allocated)
						++out->register_values;
					if (value && m_vector_registers[value] >= 0)
					{
						++out->vector_register_values;
						if (m_vector_homes[value])
							++out->vector_save_values;
					}
					if (value && !m_constants[value] && (!allocated || m_vector_homes[value]))
					{
						++out->spill_values;
						const u32 size = std::max(8u, ir::TypeSize(inst.type));
						auto& free = free_slots[size == 16];
						if (m_reuse_spill_slots && !free.empty())
						{
							m_slots[value] = free.back();
							free.pop_back();
						}
						else
						{
							next_slot = (next_slot + size - 1u) & ~(size - 1u);
							m_slots[value] = next_slot;
							next_slot += size;
							++out->spill_slots;
						}
						active.push_back({std::max(position, last_use[value]), size, m_slots[value]});
					}
					++position;
				}
				// SSA values cannot cross blocks; all typed slots are reusable
				// on every successor, including loops and conditional paths.
				for (const Spill& spill : active)
					free_slots[spill.size == 16].push_back(spill.slot);
			}
			m_frame = (next_slot + 15u) & ~15u;
			if (m_frame > 0x3f00)
				return Fail(error, "stack frame too large");
			out->frame_size = m_frame;

			if (m_inline)
			{
				if (!m_hooks)
					return Fail(error, "inline mode requires exit hooks");
				for (const ir::Block& block : m_fn.blocks)
				{
					for (const ir::Inst& inst : block.insts)
					{
						if ((inst.op == ir::Op::Resume && !m_hooks->guest_exit) ||
							(inst.op == ir::Op::BranchIndirect && !m_hooks->indirect_exit))
							return Fail(error, "missing inline exit hook");
					}
				}

				// Emit into the caller's active Oaknut block. The caller is
				// responsible for the code buffer and for the exit hooks.
				m_labels.resize(m_fn.blocks.size());
				u8* const start = oakGetCurrentCodePointer();
				EmitPrologue();
				EmitEntryJump();
				for (const ir::Block& block : m_fn.blocks)
				{
					oakAsm->l(m_labels[block.id - 1]);
					if (!EmitBlock(block, error))
						return false;
				}
				out->entry = nullptr;
				out->host_size = static_cast<u32>(oakGetCurrentCodePointer() - start);
				out->direct_quad_operations = m_direct_quad_operations;
				out->direct_quad_vector_operations = m_direct_quad_vector_operations;
				out->iop_memory_operations = m_iop_memory_operations;
				out->direct_iop_load_operations = m_direct_iop_load_operations;
				out->fused_multiply_pairs = m_fused_multiply_pairs;
				return true;
			}

			oakSetAsmPtr(code, capacity);
			u8* const start = oakStartBlock();

			m_labels.resize(m_fn.blocks.size());
			EmitPrologue();
			EmitEntryJump();

			for (const ir::Block& block : m_fn.blocks)
			{
				oakAsm->l(m_labels[block.id - 1]);
				if (!EmitBlock(block, error))
					return false;
			}

			const u8* const end = oakEndBlock();
			out->entry = start;
			out->host_size = static_cast<u32>(end - start);
			out->direct_quad_operations = m_direct_quad_operations;
			out->direct_quad_vector_operations = m_direct_quad_vector_operations;
			out->iop_memory_operations = m_iop_memory_operations;
			out->direct_iop_load_operations = m_direct_iop_load_operations;
			out->fused_multiply_pairs = m_fused_multiply_pairs;
			return true;
		}

		bool Lowerer::TryEmitMultiplyPair(const ir::Inst& lo, const ir::Inst& hi)
		{
			if (!m_fuse_multiply_pairs || lo.op != ir::Op::Mul || lo.type != ir::Type::I32 ||
				(hi.op != ir::Op::MulHiS && hi.op != ir::Op::MulHiU) || hi.type != ir::Type::I32 ||
				lo.args[0] != hi.args[0] || lo.args[1] != hi.args[1] ||
				m_fn.ValueType(lo.args[0]) != ir::Type::I32 || m_fn.ValueType(lo.args[1]) != ir::Type::I32)
				return false;
			// Read both inputs before defining either result, matching the existing
			// allocator's final-use coalescing contract. X2 is outside its pool.
			recBeginOaknutEmit();
			const auto lhs = Operand32(lo.args[0], oak::util::W0);
			const auto rhs = Operand32(lo.args[1], oak::util::W1);
			if (hi.op == ir::Op::MulHiS)
				oakAsm->SMULL(oak::util::X2, lhs, rhs);
			else
				oakAsm->UMULL(oak::util::X2, lhs, rhs);
			Store32(lo.value, oak::util::W2);
			const auto dst_hi = Result32(hi.value);
			oakAsm->LSR(dst_hi.toX(), oak::util::X2, 32);
			Store32(hi.value, dst_hi);
			recEndOaknutEmit();
			// Vector save/restore schedules use original IR instruction positions.
			m_emit_position += 2;
			++m_fused_multiply_pairs;
			return true;
		}

		bool Lowerer::EmitBlock(const ir::Block& block, std::string* error)
		{
			for (size_t i = 0; i < block.insts.size(); ++i)
			{
				if (i + 1 < block.insts.size() && TryEmitMultiplyPair(block.insts[i], block.insts[i + 1]))
					++i;
				else if (!EmitInst(block.insts[i], error))
					return false;
			}
			return true;
		}

		bool Lowerer::EmitInst(const ir::Inst& inst, std::string* error)
		{
			const u32 position = m_emit_position++;
			if (!m_allocate_vectors || !IsVectorBarrier(inst) || UsesDirectVectorMemory(inst))
				return EmitInstBody(inst, error);
			m_at_vector_barrier = true;
			recBeginOaknutEmit();
			SaveVectors(position);
			recEndOaknutEmit();
			if (!EmitInstBody(inst, error))
				return false;
			recBeginOaknutEmit();
			RestoreVectors(position);
			recEndOaknutEmit();
			m_at_vector_barrier = false;
			return true;
		}

		bool Lowerer::EmitInstBody(const ir::Inst& inst, std::string* error)
		{
			const u32* a = inst.args;
			if (inst.value && m_constants[inst.value])
				return true;

			if (m_inline && (inst.op == ir::Op::Trap || inst.op == ir::Op::CheckEvents ||
					inst.op == ir::Op::Return))
			{
				return Fail(error, "inline mode does not support this terminator");
			}

			auto load_a = [&]() { Load32(a[0], oak::util::W0); };
			auto load_b = [&]() { Load32(a[1], oak::util::W1); };
			auto store_r = [&]() { Store32(inst.value, oak::util::W0); };
			auto call_helper = [&](const void* fn) {
				if (m_hooks && m_hooks->before_helper)
					m_hooks->before_helper(m_hooks->ctx);
				oakEmitCall(fn);
				if (m_hooks && m_hooks->after_helper)
					m_hooks->after_helper(m_hooks->ctx);
			};

			switch (inst.op)
			{
				case ir::Op::Nop:
					return true;

				case ir::Op::ConstI32:
				case ir::Op::ConstF32:
					recBeginOaknutEmit();
					oakAsm->MOV(oak::util::W0, static_cast<u32>(inst.imm));
					store_r();
					recEndOaknutEmit();
					return true;

				case ir::Op::ConstI64:
					recBeginOaknutEmit();
					oakAsm->MOV(oak::util::X0, inst.imm);
					Store64(inst.value, oak::util::X0);
					recEndOaknutEmit();
					return true;

				case ir::Op::ConstVec:
				{
					const auto& lanes = m_fn.vec_consts[inst.imm];
					recBeginOaknutEmit();
					const auto dst = Result128(inst.value);
					oakAsm->MOV(oak::util::X0, u64(lanes[0]) | (u64(lanes[1]) << 32));
					oakAsm->MOV(oak::util::X1, u64(lanes[2]) | (u64(lanes[3]) << 32));
					if (m_vector_registers[inst.value] >= 0)
					{
						oakAsm->FMOV(dst.toD(), oak::util::X0);
						oakAsm->INS(dst.Delem()[1], oak::util::X1);
					}
					else
					{
						oakStore64(oak::util::X0, {FrameBase(), static_cast<s64>(Slot(inst.value))});
						oakStore64(oak::util::X1, {FrameBase(), static_cast<s64>(Slot(inst.value) + 8)});
					}
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::Copy:
					if (m_reuse_spill_slots && m_registers[inst.value] < 0 && m_registers[a[0]] < 0 &&
						m_vector_registers[inst.value] < 0 && m_vector_registers[a[0]] < 0 &&
						!m_constants[a[0]] && Slot(inst.value) == Slot(a[0]))
						return true;
					if (inst.type == ir::Type::V4U32)
					{
						recBeginOaknutEmit();
						Load128(a[0], Result128(inst.value));
						Store128(inst.value, Result128(inst.value));
						recEndOaknutEmit();
					}
					else if (inst.type == ir::Type::I64)
					{
						recBeginOaknutEmit();
						Load64(a[0], Result64(inst.value));
						Store64(inst.value, Result64(inst.value));
						recEndOaknutEmit();
					}
					else
					{
						recBeginOaknutEmit();
						Load32(a[0], Result32(inst.value));
						Store32(inst.value, Result32(inst.value));
						recEndOaknutEmit();
					}
					return true;

				case ir::Op::Undef:
					recBeginOaknutEmit();
					oakAsm->MOV(oak::util::W0, 0);
					store_r();
					recEndOaknutEmit();
					return true;

				case ir::Op::Select:
					recBeginOaknutEmit();
					Load32(a[0], oak::util::W0);
					if (inst.type == ir::Type::I64)
					{
						Load64(a[1], oak::util::X1);
						Load64(a[2], oak::util::X2);
						oakAsm->CMP(oak::util::W0, 0);
						oakAsm->CSEL(oak::util::X0, oak::util::X1, oak::util::X2, oak::Cond::NE);
						Store64(inst.value, oak::util::X0);
					}
					else
					{
						Load32(a[1], oak::util::W1);
						Load32(a[2], oak::util::W2);
						oakAsm->CMP(oak::util::W0, 0);
						oakAsm->CSEL(oak::util::W0, oak::util::W1, oak::util::W2, oak::Cond::NE);
						store_r();
					}
					recEndOaknutEmit();
					return true;

				case ir::Op::Not:
					recBeginOaknutEmit();
					if (inst.type == ir::Type::V4U32)
					{
						const auto src = Operand128(a[0], oak::util::Q0);
						const auto dst = Result128(inst.value);
						oakAsm->NOT(dst.B16(), src.B16());
						Store128(inst.value, dst);
					}
					else if (inst.type == ir::Type::I64)
					{
						const auto src = Operand64(a[0], oak::util::X0);
						const auto dst = Result64(inst.value);
						oakAsm->MVN(dst, src);
						Store64(inst.value, dst);
					}
					else
					{
						const auto src = Operand32(a[0], oak::util::W0);
						const auto dst = Result32(inst.value);
						oakAsm->MVN(dst, src);
						Store32(inst.value, dst);
					}
					recEndOaknutEmit();
					return true;

				case ir::Op::Neg:
				case ir::Op::Sext8:
				case ir::Op::Sext16:
				case ir::Op::Zext8:
				case ir::Op::Zext16:
				{
					recBeginOaknutEmit();
					const auto src = Operand32(a[0], oak::util::W0);
					const auto dst = Result32(inst.value);
					switch (inst.op)
					{
						case ir::Op::Neg: oakAsm->NEG(dst, src); break;
						case ir::Op::Sext8: oakAsm->SXTB(dst, src); break;
						case ir::Op::Sext16: oakAsm->SXTH(dst, src); break;
						case ir::Op::Zext8: oakAsm->UXTB(dst, src); break;
						default: oakAsm->UXTH(dst, src); break;
					}
					Store32(inst.value, dst);
					recEndOaknutEmit();
					return true;
				}
				case ir::Op::Sext32:
				{
					recBeginOaknutEmit();
					const auto src = Operand32(a[0], oak::util::W0);
					const auto dst = Result64(inst.value);
					oakAsm->SXTW(dst, src);
					Store64(inst.value, dst);
					recEndOaknutEmit();
					return true;
				}
				case ir::Op::Zext32:
				{
					recBeginOaknutEmit();
					// A W-register write zeroes the upper word, including in-place.
					Load32(a[0], oak::WReg(Result64(inst.value).index()));
					Store64(inst.value, Result64(inst.value));
					recEndOaknutEmit();
					return true;
				}
				case ir::Op::Trunc32:
					recBeginOaknutEmit();
					Load32(a[0], Result32(inst.value));
					Store32(inst.value, Result32(inst.value));
					recEndOaknutEmit();
					return true;

				case ir::Op::Add:
				case ir::Op::Sub:
				case ir::Op::And:
				case ir::Op::Or:
				case ir::Op::Xor:
				case ir::Op::MinS:
				case ir::Op::MinU:
				case ir::Op::MaxS:
				case ir::Op::MaxU:
				{
					recBeginOaknutEmit();
					if (inst.type == ir::Type::V4U32)
					{
						// All operand registers are read before a coalesced result is written.
						const auto lhs = Operand128(a[0], oak::util::Q0);
						const auto rhs = Operand128(a[1], oak::util::Q1);
						const auto dst = Result128(inst.value);
						switch (inst.op)
						{
							case ir::Op::Add: oakAsm->ADD(dst.S4(), lhs.S4(), rhs.S4()); break;
							case ir::Op::Sub: oakAsm->SUB(dst.S4(), lhs.S4(), rhs.S4()); break;
							case ir::Op::And: oakAsm->AND(dst.B16(), lhs.B16(), rhs.B16()); break;
							case ir::Op::Or: oakAsm->ORR(dst.B16(), lhs.B16(), rhs.B16()); break;
							case ir::Op::Xor: oakAsm->EOR(dst.B16(), lhs.B16(), rhs.B16()); break;
							default: return Fail(error, "unsupported vector arithmetic op");
						}
						Store128(inst.value, dst);
					}
					else if (inst.type == ir::Type::I64)
					{
						const auto lhs = Operand64(a[0], oak::util::X0);
						const auto dst = Result64(inst.value);
						if (m_constants[a[1]] && EmitAddSubImmediate(inst, dst, lhs, *m_constants[a[1]]))
						{
							Store64(inst.value, dst);
							recEndOaknutEmit();
							return true;
						}
						const auto rhs = Operand64(a[1], oak::util::X1);
						switch (inst.op)
						{
							case ir::Op::Add: oakAsm->ADD(dst, lhs, rhs); break;
							case ir::Op::Sub: oakAsm->SUB(dst, lhs, rhs); break;
							case ir::Op::And: oakAsm->AND(dst, lhs, rhs); break;
							case ir::Op::Or: oakAsm->ORR(dst, lhs, rhs); break;
							case ir::Op::Xor: oakAsm->EOR(dst, lhs, rhs); break;
							default: return Fail(error, "unsupported arithmetic op");
						}
						Store64(inst.value, dst);
					}
					else
					{
						const auto lhs = Operand32(a[0], oak::util::W0);
						const auto dst = Result32(inst.value);
						if (m_constants[a[1]] && EmitAddSubImmediate(inst, dst, lhs, *m_constants[a[1]]))
						{
							Store32(inst.value, dst);
							recEndOaknutEmit();
							return true;
						}
						const auto rhs = Operand32(a[1], oak::util::W1);
						switch (inst.op)
						{
							case ir::Op::Add: oakAsm->ADD(dst, lhs, rhs); break;
							case ir::Op::Sub: oakAsm->SUB(dst, lhs, rhs); break;
							case ir::Op::And: oakAsm->AND(dst, lhs, rhs); break;
							case ir::Op::Or: oakAsm->ORR(dst, lhs, rhs); break;
							case ir::Op::Xor: oakAsm->EOR(dst, lhs, rhs); break;
							case ir::Op::MinS:
								oakAsm->CMP(lhs, rhs);
								oakAsm->CSEL(dst, lhs, rhs, oak::Cond::LT);
								break;
							case ir::Op::MinU:
								oakAsm->CMP(lhs, rhs);
								oakAsm->CSEL(dst, lhs, rhs, oak::Cond::LO);
								break;
							case ir::Op::MaxS:
								oakAsm->CMP(lhs, rhs);
								oakAsm->CSEL(dst, lhs, rhs, oak::Cond::GT);
								break;
							case ir::Op::MaxU:
								oakAsm->CMP(lhs, rhs);
								oakAsm->CSEL(dst, lhs, rhs, oak::Cond::HI);
								break;
							default: return Fail(error, "unsupported arithmetic op");
						}
						Store32(inst.value, dst);
					}
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::Mul:
					recBeginOaknutEmit();
					if (inst.type == ir::Type::I64)
					{
						const auto lhs = Operand64(a[0], oak::util::X0);
						const auto rhs = Operand64(a[1], oak::util::X1);
						const auto dst = Result64(inst.value);
						oakAsm->MUL(dst, lhs, rhs);
						Store64(inst.value, dst);
					}
					else
					{
						const auto lhs = Operand32(a[0], oak::util::W0);
						const auto rhs = Operand32(a[1], oak::util::W1);
						const auto dst = Result32(inst.value);
						oakAsm->MUL(dst, lhs, rhs);
						Store32(inst.value, dst);
					}
					recEndOaknutEmit();
					return true;

				case ir::Op::MulHiS:
				case ir::Op::MulHiU:
				{
					recBeginOaknutEmit();
					const auto lhs = Operand32(a[0], oak::util::W0);
					const auto rhs = Operand32(a[1], oak::util::W1);
					if (inst.op == ir::Op::MulHiS)
						oakAsm->SMULL(oak::util::X2, lhs, rhs);
					else
						oakAsm->UMULL(oak::util::X2, lhs, rhs);
					const auto dst = Result32(inst.value);
					oakAsm->LSR(dst.toX(), oak::util::X2, 32);
					Store32(inst.value, dst);
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::VShuffle:
				{
					recBeginOaknutEmit();
					const auto src = Operand128(a[0], oak::util::Q0);
					const auto dst = Result128(inst.value);
					const u32 selectors = static_cast<u32>(inst.imm);
					if (selectors == 0xe4) // identity: 0,1,2,3
					{
						if (src.index() != dst.index())
							oakAsm->MOV(dst.B16(), src.B16());
					}
					else if (selectors == (selectors & 3) * 0x55) // broadcast
						oakAsm->DUP(dst.S4(), src.Selem()[selectors & 3]);
					else if (selectors == 0xc6) // 2,1,0,3 (PEXEW)
					{
						oakAsm->REV64(dst.S4(), src.S4());
						oakAsm->EXT(dst.B16(), dst.B16(), dst.B16(), 12);
					}
					else if (selectors == 0xc9) // 1,2,0,3 (PROT3W)
					{
						oakAsm->REV64(oak::util::Q1.S4(), src.S4());
						oakAsm->EXT(oak::util::Q0.B16(), src.B16(), src.B16(), 8);
						oakAsm->ZIP1(dst.S4(), oak::util::Q1.S4(), oak::util::Q0.S4());
					}
					else if (selectors == 0xd8) // 0,2,1,3 (PEXCW)
					{
						oakAsm->EXT(oak::util::Q1.B16(), src.B16(), src.B16(), 8);
						oakAsm->ZIP1(dst.S4(), src.S4(), oak::util::Q1.S4());
					}
					else
					{
						// Snapshot first: dst may reuse the dying input's register.
						auto input = src;
						if (src.index() == dst.index())
						{
							oakAsm->MOV(oak::util::Q1.B16(), src.B16());
							input = oak::util::Q1;
						}
						oakAsm->DUP(dst.S4(), input.Selem()[selectors & 3]);
						for (u32 lane = 1; lane < 4; ++lane)
							oakAsm->INS(dst.Selem()[lane], input.Selem()[(selectors >> (lane * 2)) & 3]);
					}
					Store128(inst.value, dst);
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::VShuffle2:
				{
					recBeginOaknutEmit();
					const auto lhs = Operand128(a[0], oak::util::Q0);
					const auto rhs = Operand128(a[1], oak::util::Q1);
					const auto dst = Result128(inst.value);
					switch (inst.imm)
					{
						case 0xa60: oakAsm->ZIP1(dst.S4(), lhs.S4(), rhs.S4()); break; // 0,4,1,5
						case 0xef2: oakAsm->ZIP2(dst.S4(), lhs.S4(), rhs.S4()); break; // 2,6,3,7
						case 0xd10: oakAsm->UZP1(dst.S4(), lhs.S4(), rhs.S4()); break; // 0,2,4,6
						case 0xb08: oakAsm->ZIP1(dst.D2(), lhs.D2(), rhs.D2()); break; // 0,1,4,5
						case 0x6be: oakAsm->ZIP2(dst.D2(), rhs.D2(), lhs.D2()); break; // 6,7,2,3
						case 0x688: // 0,1,2,3
							if (dst.index() != lhs.index()) oakAsm->MOV(dst.B16(), lhs.B16());
							break;
						case 0xfac: // 4,5,6,7
							if (dst.index() != rhs.index()) oakAsm->MOV(dst.B16(), rhs.B16());
							break;
						default:
							// Capture every source word before touching a coalesced dst.
							for (u32 lane = 0; lane < 4; ++lane)
							{
								const u32 select = (inst.imm >> (lane * 3)) & 7;
								oakAsm->UMOV(oak::WReg(lane), (select < 4 ? lhs : rhs).Selem()[select & 3]);
							}
							for (u32 lane = 0; lane < 4; ++lane)
								oakAsm->INS(dst.Selem()[lane], oak::WReg(lane));
							break;
					}
					Store128(inst.value, dst);
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::VMinS:
				case ir::Op::VMinU:
				case ir::Op::VMaxS:
				case ir::Op::VMaxU:
				case ir::Op::VCmpEq:
				case ir::Op::VCmpNe:
				case ir::Op::VCmpLtS:
				case ir::Op::VCmpLtU:
				case ir::Op::VCmpLeS:
				case ir::Op::VCmpLeU:
				{
					recBeginOaknutEmit();
					const auto lhs = Operand128(a[0], oak::util::Q0);
					const auto rhs = Operand128(a[1], oak::util::Q1);
					const auto dst = Result128(inst.value);
					switch (inst.op)
					{
						case ir::Op::VMinS: oakAsm->SMIN(dst.S4(), lhs.S4(), rhs.S4()); break;
						case ir::Op::VMinU: oakAsm->UMIN(dst.S4(), lhs.S4(), rhs.S4()); break;
						case ir::Op::VMaxS: oakAsm->SMAX(dst.S4(), lhs.S4(), rhs.S4()); break;
						case ir::Op::VMaxU: oakAsm->UMAX(dst.S4(), lhs.S4(), rhs.S4()); break;
						case ir::Op::VCmpEq: oakAsm->CMEQ(dst.S4(), lhs.S4(), rhs.S4()); break;
						case ir::Op::VCmpNe:
							oakAsm->CMEQ(dst.S4(), lhs.S4(), rhs.S4());
							oakAsm->NOT(dst.B16(), dst.B16());
							break;
						case ir::Op::VCmpLtS: oakAsm->CMGT(dst.S4(), rhs.S4(), lhs.S4()); break;
						case ir::Op::VCmpLtU: oakAsm->CMHI(dst.S4(), rhs.S4(), lhs.S4()); break;
						case ir::Op::VCmpLeS: oakAsm->CMGE(dst.S4(), rhs.S4(), lhs.S4()); break;
						case ir::Op::VCmpLeU: oakAsm->CMHS(dst.S4(), rhs.S4(), lhs.S4()); break;
						default: break;
					}
					Store128(inst.value, dst);
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::VShl:
				case ir::Op::VShrU:
				case ir::Op::VShrS:
				{
					recBeginOaknutEmit();
					const auto source = Operand128(a[0], oak::util::Q0);
					const auto dst = Result128(inst.value);
					if (m_constants[a[1]])
					{
						const u32 amount = static_cast<u32>(*m_constants[a[1]]) & 31u;
						// ARM64 immediate right shifts require a nonzero amount.
						if (amount == 0)
						{
							if (dst.index() != source.index())
								oakAsm->MOV(dst.B16(), source.B16());
						}
						else if (inst.op == ir::Op::VShl)
							oakAsm->SHL(dst.S4(), source.S4(), amount);
						else if (inst.op == ir::Op::VShrU)
							oakAsm->USHR(dst.S4(), source.S4(), amount);
						else
							oakAsm->SSHR(dst.S4(), source.S4(), amount);
					}
					else
					{
						Load32(a[1], oak::util::W0);
						oakAsm->AND(oak::util::W0, oak::util::W0, 31);
						if (inst.op != ir::Op::VShl)
							oakAsm->NEG(oak::util::W0, oak::util::W0);
						oakAsm->DUP(oak::util::Q1.S4(), oak::util::W0);
						if (inst.op == ir::Op::VShrS)
							oakAsm->SSHL(dst.S4(), source.S4(), oak::util::Q1.S4());
						else
							oakAsm->USHL(dst.S4(), source.S4(), oak::util::Q1.S4());
					}
					Store128(inst.value, dst);
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::Shl:
				case ir::Op::ShrU:
				case ir::Op::ShrS:
				{
					recBeginOaknutEmit();
					if (inst.type == ir::Type::I64)
					{
						const auto lhs = Operand64(a[0], oak::util::X0);
						if (m_constants[a[1]])
						{
							const auto dst = Result64(inst.value);
							const u32 amount = static_cast<u32>(*m_constants[a[1]]) & 63u;
							if (inst.op == ir::Op::Shl)
								oakAsm->LSL(dst, lhs, amount);
							else if (inst.op == ir::Op::ShrU)
								oakAsm->LSR(dst, lhs, amount);
							else
								oakAsm->ASR(dst, lhs, amount);
							Store64(inst.value, dst);
							recEndOaknutEmit();
							return true;
						}
						const auto rhs = m_fn.ValueType(a[1]) == ir::Type::I64 ?
							Operand64(a[1], oak::util::X1) : oak::XReg(Operand32(a[1], oak::util::W1).index());
						const auto dst = Result64(inst.value);
						if (inst.op == ir::Op::Shl)
							oakAsm->LSLV(dst, lhs, rhs);
						else if (inst.op == ir::Op::ShrU)
							oakAsm->LSRV(dst, lhs, rhs);
						else
							oakAsm->ASRV(dst, lhs, rhs);
						Store64(inst.value, dst);
					}
					else
					{
						const auto lhs = Operand32(a[0], oak::util::W0);
						if (m_constants[a[1]])
						{
							const auto dst = Result32(inst.value);
							const u32 amount = static_cast<u32>(*m_constants[a[1]]) & 31u;
							if (inst.op == ir::Op::Shl)
								oakAsm->LSL(dst, lhs, amount);
							else if (inst.op == ir::Op::ShrU)
								oakAsm->LSR(dst, lhs, amount);
							else
								oakAsm->ASR(dst, lhs, amount);
							Store32(inst.value, dst);
							recEndOaknutEmit();
							return true;
						}
						const auto rhs = Operand32(a[1], oak::util::W1);
						const auto dst = Result32(inst.value);
						if (inst.op == ir::Op::Shl)
							oakAsm->LSLV(dst, lhs, rhs);
						else if (inst.op == ir::Op::ShrU)
							oakAsm->LSRV(dst, lhs, rhs);
						else
							oakAsm->ASRV(dst, lhs, rhs);
						Store32(inst.value, dst);
					}
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::CmpEq:
				case ir::Op::CmpNe:
				case ir::Op::CmpLtS:
				case ir::Op::CmpLtU:
				case ir::Op::CmpLeS:
				case ir::Op::CmpLeU:
				case ir::Op::CmpGtS:
				case ir::Op::CmpGtU:
				case ir::Op::CmpGeS:
				case ir::Op::CmpGeU:
				{
					oak::Cond cond = oak::Cond::EQ;
					switch (inst.op)
					{
						case ir::Op::CmpEq: cond = oak::Cond::EQ; break;
						case ir::Op::CmpNe: cond = oak::Cond::NE; break;
						case ir::Op::CmpLtS: cond = oak::Cond::LT; break;
						case ir::Op::CmpLtU: cond = oak::Cond::LO; break;
						case ir::Op::CmpLeS: cond = oak::Cond::LE; break;
						case ir::Op::CmpLeU: cond = oak::Cond::LS; break;
						case ir::Op::CmpGtS: cond = oak::Cond::GT; break;
						case ir::Op::CmpGtU: cond = oak::Cond::HI; break;
						case ir::Op::CmpGeS: cond = oak::Cond::GE; break;
						default: cond = oak::Cond::HS; break;
					}
					recBeginOaknutEmit();
					if (m_fn.ValueType(a[0]) == ir::Type::I64)
					{
						const auto lhs = Operand64(a[0], oak::util::X0);
						const auto rhs = Operand64(a[1], oak::util::X1);
						oakAsm->CMP(lhs, rhs);
					}
					else
					{
						const auto lhs = Operand32(a[0], oak::util::W0);
						const auto rhs = Operand32(a[1], oak::util::W1);
						oakAsm->CMP(lhs, rhs);
					}
					const auto dst = Result32(inst.value);
					oakAsm->CSET(dst, cond);
					Store32(inst.value, dst);
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::Msub:
				{
					recBeginOaknutEmit();
					if (inst.type == ir::Type::I64)
					{
						const auto lhs = Operand64(a[0], oak::util::X0);
						const auto rhs = Operand64(a[1], oak::util::X1);
						const auto addend = Operand64(a[2], oak::util::X2);
						const auto dst = Result64(inst.value);
						oakAsm->MSUB(dst, lhs, rhs, addend);
						Store64(inst.value, dst);
					}
					else
					{
						const auto lhs = Operand32(a[0], oak::util::W0);
						const auto rhs = Operand32(a[1], oak::util::W1);
						const auto addend = Operand32(a[2], oak::util::W2);
						const auto dst = Result32(inst.value);
						oakAsm->MSUB(dst, lhs, rhs, addend);
						Store32(inst.value, dst);
					}
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::DivS:
				case ir::Op::RemS:
				case ir::Op::DivU:
				case ir::Op::RemU:
				{
					if (m_inline_division)
					{
						recBeginOaknutEmit();
						const auto lhs = Operand32(a[0], oak::util::W0);
						const auto rhs = Operand32(a[1], oak::util::W1);
						const auto dst = Result32(inst.value);
						const bool sign = inst.op == ir::Op::DivS || inst.op == ir::Op::RemS;
						// Keep the quotient in scratch: coalescing may assign dst
						// to an operand that is still needed for the remainder or
						// the R5900 divide-by-zero correction below.
						if (sign)
							oakAsm->SDIV(oak::util::W2, lhs, rhs);
						else
							oakAsm->UDIV(oak::util::W2, lhs, rhs);
						if (inst.op == ir::Op::RemS || inst.op == ir::Op::RemU)
						{
							// ARM64's zero-divisor quotient is zero, so MSUB also
							// returns the dividend in that case. INT_MIN / -1
							// wraps to INT_MIN and produces a zero remainder.
							oakAsm->MSUB(dst, oak::util::W2, rhs, lhs);
						}
						else
						{
							if (sign)
							{
								// R5900 signed divide by zero: negative -> 1,
								// nonnegative -> -1, including a zero dividend.
								oakAsm->ASR(oak::util::W3, lhs, 31);
								oakAsm->LSL(oak::util::W3, oak::util::W3, 1);
								oakAsm->MVN(oak::util::W3, oak::util::W3);
							}
							else
								oakAsm->MOV(oak::util::W3, 0xffffffffu);
							oakAsm->CMP(rhs, 0);
							oakAsm->CSEL(dst, oak::util::W3, oak::util::W2, oak::Cond::EQ);
						}
						Store32(inst.value, dst);
						recEndOaknutEmit();
						return true;
					}
					const void* helper = nullptr;
					switch (inst.op)
					{
						case ir::Op::DivS: helper = reinterpret_cast<const void*>(&EeIrDivSLo); break;
						case ir::Op::RemS: helper = reinterpret_cast<const void*>(&EeIrDivSHi); break;
						case ir::Op::DivU: helper = reinterpret_cast<const void*>(&EeIrDivULo); break;
						default: helper = reinterpret_cast<const void*>(&EeIrDivUHi); break;
					}
					recBeginOaknutEmit();
					load_a();
					load_b();
					call_helper(helper);
					store_r();
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::Load8U:
				case ir::Op::Load8S:
				case ir::Op::Load16U:
				case ir::Op::Load16S:
				case ir::Op::Load32:
				{
					const bool iop = m_guest_state == GuestState::IOP;
					if (iop) ++m_iop_memory_operations;
					const void* helper = nullptr;
					switch (inst.op)
					{
						case ir::Op::Load8U: helper = iop ? reinterpret_cast<const void*>(&IopIrMemRead8) : reinterpret_cast<const void*>(&EeIrMemRead8); break;
						case ir::Op::Load8S: helper = iop ? reinterpret_cast<const void*>(&IopIrMemRead8S) : reinterpret_cast<const void*>(&EeIrMemRead8S); break;
						case ir::Op::Load16U: helper = iop ? reinterpret_cast<const void*>(&IopIrMemRead16) : reinterpret_cast<const void*>(&EeIrMemRead16); break;
						case ir::Op::Load16S: helper = iop ? reinterpret_cast<const void*>(&IopIrMemRead16S) : reinterpret_cast<const void*>(&EeIrMemRead16S); break;
						default: helper = iop ? reinterpret_cast<const void*>(&IopIrMemRead32) : reinterpret_cast<const void*>(&EeIrMemRead32); break;
					}
					recBeginOaknutEmit();
					load_a();
					if (m_hooks && m_hooks->before_memory)
						m_hooks->before_memory(m_hooks->ctx, inst.guest_pc, (inst.aux & ir::IF_DELAY_SLOT) != 0);
					oak::Label slow, done;
					if (iop && m_direct_iop_loads)
					{
						// Only real RAM pages bypass handlers. Read the live LUT at
						// execution time so remaps and null pages retain their meaning.
						oakAsm->AND(oak::util::W3, oak::util::W0, 0x1fffffff);
						oakAsm->CMP(oak::util::W3, 0x00800000);
						oakAsm->B(oak::Cond::HS, slow);
						oakMoveAddressToReg(oak::util::X2, &psxMemRLUT);
						oakLoad64(oak::util::X2, {oak::util::X2, 0});
						oakAsm->LSR(oak::util::W4, oak::util::W3, 16);
						oakAsm->LDR(oak::util::X2, oak::util::X2, oak::util::X4, oak::IndexExt::LSL, 3);
						oakAsm->CBZ(oak::util::X2, slow);
						oakAsm->AND(oak::util::W3, oak::util::W3, 0xffff);
						oakAsm->ADD(oak::util::X2, oak::util::X2, oak::util::X3);
						switch (inst.op)
						{
							case ir::Op::Load8U: oakAsm->LDRB(oak::util::W0, oak::util::X2); break;
							case ir::Op::Load8S: oakAsm->LDRSB(oak::util::W0, oak::util::X2); break;
							case ir::Op::Load16U: oakAsm->LDRH(oak::util::W0, oak::util::X2); break;
							case ir::Op::Load16S: oakAsm->LDRSH(oak::util::W0, oak::util::X2); break;
							default: oakAsm->LDR(oak::util::W0, oak::util::X2); break;
						}
						oakAsm->B(done);
						oakAsm->l(slow);
						++m_direct_iop_load_operations;
					}
					call_helper(helper);
					if (iop && m_direct_iop_loads)
						oakAsm->l(done);
					if (m_hooks && m_hooks->after_memory)
						m_hooks->after_memory(m_hooks->ctx);
					store_r();
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::Load64:
					recBeginOaknutEmit();
					load_a();
					if (m_hooks && m_hooks->before_memory)
						m_hooks->before_memory(m_hooks->ctx, inst.guest_pc, (inst.aux & ir::IF_DELAY_SLOT) != 0);
					call_helper(reinterpret_cast<const void*>(&EeIrMemRead64));
					if (m_hooks && m_hooks->after_memory)
						m_hooks->after_memory(m_hooks->ctx);
					Store64(inst.value, oak::util::X0);
					recEndOaknutEmit();
					return true;

				case ir::Op::Load128:
				case ir::Op::Store128:
				{
					const bool direct_vectors = UsesDirectVectorMemory(inst);
					const u32 position = m_emit_position - 1;
					recBeginOaknutEmit();
					load_a();
					if (m_hooks && m_hooks->before_memory)
						m_hooks->before_memory(m_hooks->ctx, inst.guest_pc, (inst.aux & ir::IF_DELAY_SLOT) != 0);
					oak::Label slow, done;
					if (m_direct_quad_memory)
					{
						// VTLBVirtual stores host_pointer - guest_address. Test
						// the sign AFTER adding the zero-extended address: entries
						// for high guest aliases can themselves be negative.
						oakLoad64(oak::util::X2, {GuestBase(), static_cast<s64>(offsetof(cpuRegistersPack, vtlbdata.vmap))});
						oakAsm->LSR(oak::util::W3, oak::util::W0, vtlb_private::VTLB_PAGE_BITS);
						oakAsm->LDR(oak::util::X2, oak::util::X2, oak::util::X3, oak::IndexExt::LSL, 3);
						oakAsm->ADD(oak::util::X2, oak::util::X2, oak::util::X0);
						oakAsm->TBNZ(oak::util::X2, 63, slow);
						if (inst.op == ir::Op::Load128)
						{
							const auto dst = Result128(inst.value);
							oakAsm->LDR(dst, oak::util::X2);
							Store128(inst.value, dst);
						}
						else
						{
							const auto src = Operand128(a[1], oak::util::Q0);
							oakAsm->STR(src, oak::util::X2);
						}
						oakAsm->B(done);
						oakAsm->l(slow);
						++m_direct_quad_operations;
						if (direct_vectors)
						{
							++m_direct_quad_vector_operations;
							// Only the C-helper fallback needs caller-save homes.
							// Save before before_helper can clobber any full Q value.
							SaveVectors(position);
						}
					}
					SlotAddress(inst.op == ir::Op::Load128 ? inst.value : a[1], oak::util::X1);
					call_helper(inst.op == ir::Op::Load128 ? reinterpret_cast<const void*>(&EeIrMemRead128) :
						reinterpret_cast<const void*>(&EeIrMemWrite128));
					if (direct_vectors)
						RestoreVectors(position);
					if (m_direct_quad_memory)
						oakAsm->l(done);
					if (m_hooks && m_hooks->after_memory)
						m_hooks->after_memory(m_hooks->ctx);
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::Store8:
				case ir::Op::Store16:
				case ir::Op::Store32:
				case ir::Op::Store64:
				{
					const bool iop = m_guest_state == GuestState::IOP;
					if (iop) ++m_iop_memory_operations;
					const void* helper = nullptr;
					switch (inst.op)
					{
						case ir::Op::Store8: helper = iop ? reinterpret_cast<const void*>(&IopIrMemWrite8) : reinterpret_cast<const void*>(&EeIrMemWrite8); break;
						case ir::Op::Store16: helper = iop ? reinterpret_cast<const void*>(&IopIrMemWrite16) : reinterpret_cast<const void*>(&EeIrMemWrite16); break;
						case ir::Op::Store32: helper = iop ? reinterpret_cast<const void*>(&IopIrMemWrite32) : reinterpret_cast<const void*>(&EeIrMemWrite32); break;
						default: helper = reinterpret_cast<const void*>(&EeIrMemWrite64); break;
					}
					recBeginOaknutEmit();
					Load32(a[0], oak::util::W0);
					if (inst.op == ir::Op::Store64)
						Load64(a[1], oak::util::X1);
					else
						Load32(a[1], oak::util::W1);
					if (m_hooks && m_hooks->before_memory)
						m_hooks->before_memory(m_hooks->ctx, inst.guest_pc, (inst.aux & ir::IF_DELAY_SLOT) != 0);
					call_helper(helper);
					if (m_hooks && m_hooks->after_memory)
						m_hooks->after_memory(m_hooks->ctx);
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::ReadCp0:
					recBeginOaknutEmit();
					oakLoad32(Result32(inst.value), {GuestBase(), static_cast<s64>(offsetof(cpuRegistersPack, psxRegs.CP0) + inst.imm * sizeof(u32))});
					Store32(inst.value, Result32(inst.value));
					recEndOaknutEmit();
					return true;

				case ir::Op::WriteCp0:
					recBeginOaknutEmit();
					oakStore32(Operand32(a[0], oak::util::W0), {GuestBase(), static_cast<s64>(offsetof(cpuRegistersPack, psxRegs.CP0) + inst.imm * sizeof(u32))});
					recEndOaknutEmit();
					return true;

				case ir::Op::CheckInterrupts:
					recBeginOaknutEmit();
					call_helper(reinterpret_cast<const void*>(&iopTestIntc));
					recEndOaknutEmit();
					return true;

				case ir::Op::ReadGpr:
					recBeginOaknutEmit();
					if (inst.type == ir::Type::V4U32)
					{
						const auto dst = Result128(inst.value);
						if (inst.imm == 0)
							oakAsm->MOVI(dst.B16(), 0);
						else
							oakLoad128(dst, {GuestBase(), GprOffset(static_cast<u32>(inst.imm))});
						Store128(inst.value, dst);
					}
					else if (inst.type == ir::Type::I64)
					{
						oakLoad64(Result64(inst.value), {GuestBase(), GprOffset(static_cast<u32>(inst.imm))});
						Store64(inst.value, Result64(inst.value));
					}
					else
					{
						if (inst.imm == 0)
							oakAsm->MOV(Result32(inst.value), 0);
						else
							oakLoad32(Result32(inst.value), {GuestBase(), GprOffset(static_cast<u32>(inst.imm))});
						Store32(inst.value, Result32(inst.value));
					}
					recEndOaknutEmit();
					return true;

				case ir::Op::WriteGpr:
					if (inst.imm == 0)
						return true; // $0 is hardwired to zero
					recBeginOaknutEmit();
					if (m_fn.ValueType(a[0]) == ir::Type::V4U32)
					{
						const auto src = Operand128(a[0], oak::util::Q0);
						oakStore128(src, {GuestBase(), GprOffset(static_cast<u32>(inst.imm))});
						recEndOaknutEmit();
						return true;
					}
					if (inst.aux & ir::IF_WIDE_WRITE)
					{
						const auto value = Operand64(a[0], oak::util::X0);
						oakStore64(value, {GuestBase(), GprOffset(static_cast<u32>(inst.imm))});
						recEndOaknutEmit();
						return true;
					}
					else
					{
						const auto value = Operand32(a[0], oak::util::W0);
						if (m_guest_state == GuestState::IOP)
						{
							oakStore32(value, {GuestBase(), GprOffset(static_cast<u32>(inst.imm))});
							recEndOaknutEmit();
							return true;
						}
						oakAsm->SXTW(oak::util::X0, value);
					}
					oakStore64(oak::util::X0, {GuestBase(), GprOffset(static_cast<u32>(inst.imm))});
					recEndOaknutEmit();
					return true;

				case ir::Op::ReadHi:
				case ir::Op::ReadLo:
					recBeginOaknutEmit();
					if (inst.type == ir::Type::I64)
					{
						oakLoad64(Result64(inst.value), {GuestBase(), (inst.op == ir::Op::ReadHi) ? HiOffset() : LoOffset()});
						Store64(inst.value, Result64(inst.value));
					}
					else
					{
						oakLoad32(Result32(inst.value), {GuestBase(), (inst.op == ir::Op::ReadHi) ? HiOffset() : LoOffset()});
						Store32(inst.value, Result32(inst.value));
					}
					recEndOaknutEmit();
					return true;

				case ir::Op::WriteHi:
				case ir::Op::WriteLo:
					recBeginOaknutEmit();
					if (m_fn.ValueType(a[0]) == ir::Type::I64)
					{
						const auto value = Operand64(a[0], oak::util::X0);
						oakStore64(value, {GuestBase(), (inst.op == ir::Op::WriteHi) ? HiOffset() : LoOffset()});
						recEndOaknutEmit();
						return true;
					}
					else
					{
						const auto value = Operand32(a[0], oak::util::W0);
						if (m_guest_state == GuestState::IOP)
						{
							oakStore32(value, {GuestBase(), (inst.op == ir::Op::WriteHi) ? HiOffset() : LoOffset()});
							recEndOaknutEmit();
							return true;
						}
						oakAsm->SXTW(oak::util::X0, value);
					}
					oakStore64(oak::util::X0, {GuestBase(), (inst.op == ir::Op::WriteHi) ? HiOffset() : LoOffset()});
					recEndOaknutEmit();
					return true;

				case ir::Op::AddCycles:
					return true; // M1b does not model cycle accounting yet.

				case ir::Op::Jump:
					recBeginOaknutEmit();
					oakAsm->B(m_labels[a[0] - 1]);
					recEndOaknutEmit();
					return true;

				case ir::Op::Branch:
				{
					const u32 taken = a[1];
					const u32 not_taken = a[2];
					recBeginOaknutEmit();
					const auto condition = Operand32(a[0], oak::util::W0);
					oakAsm->CBNZ(condition, m_labels[taken - 1]);
					oakAsm->B(m_labels[not_taken - 1]);
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::BranchIndirect:
					if (m_inline)
					{
						// Address arrives in W16 for the integration hook.
						recBeginOaknutEmit();
						Load32(a[0], oak::util::W16);
						EmitEpilogue();
						recEndOaknutEmit();
						m_hooks->indirect_exit(m_hooks->ctx);
						return true;
					}
					recBeginOaknutEmit();
					load_a();
					if (m_capture_exit)
					{
						oakMoveAddressToReg(oak::util::X16, &g_eeir_exit_valid);
						oakAsm->MOV(oak::util::W17, 1);
						oakStore32(oak::util::W17, {oak::util::X16, 0});
						oakMoveAddressToReg(oak::util::X16, &g_eeir_exit_pc);
						oakStore32(oak::util::W0, {oak::util::X16, 0});
					}
					oakStore32(oak::util::W0, {GuestBase(), PcOffset()});
					EmitEpilogue();
					recEndOaknutEmit();
					return true;

				case ir::Op::Resume:
					if (m_inline)
					{
						EmitEpilogue();
						m_hooks->guest_exit(m_hooks->ctx, static_cast<u32>(inst.imm), (inst.aux & ir::IF_ANNULLED_DELAY_SLOT) != 0);
						return true;
					}
					if (m_capture_exit)
					{
						recBeginOaknutEmit();
						oakMoveAddressToReg(oak::util::X16, &g_eeir_exit_valid);
						oakAsm->MOV(oak::util::W17, 1);
						oakStore32(oak::util::W17, {oak::util::X16, 0});
						oakMoveAddressToReg(oak::util::X16, &g_eeir_exit_pc);
						oakAsm->MOV(oak::util::W17, static_cast<u32>(inst.imm));
						oakStore32(oak::util::W17, {oak::util::X16, 0});
						recEndOaknutEmit();
					}
					EmitEpilogue();
					return true;

				case ir::Op::Trap:
				case ir::Op::CheckEvents:
				case ir::Op::Return:
					EmitEpilogue();
					return true;

				default:
					return Fail(error, "unsupported IR instruction");
			}
		}
	} // namespace

	bool CanLower(const ir::Function& fn, bool inline_body, std::string* error, GuestState guest_state)
	{
		if (!ir::Verify(fn, error))
			return false;
		// Reject before emission: the caller may fall back to the legacy JIT.
		u64 worst_frame = 64;
		for (u32 value = 1; value < fn.value_types.size(); ++value)
		{
			const u32 size = std::max(8u, ir::TypeSize(fn.ValueType(value)));
			worst_frame = ((worst_frame + size - 1u) & ~u64(size - 1u)) + size;
		}
		if (((worst_frame + 15u) & ~u64(15)) > 0x3f00u)
		{
			if (error)
				*error = "IR stack frame too large";
			return false;
		}
		for (const ir::Block& block : fn.blocks)
		{
			for (const ir::Inst& inst : block.insts)
			{
				if (guest_state != GuestState::IOP && (inst.op == ir::Op::ReadCp0 || inst.op == ir::Op::WriteCp0 || inst.op == ir::Op::CheckInterrupts))
				{
					if (error) *error = "COP0/interrupt lowering is only modelled for IOP";
					return false;
				}
				if (guest_state == GuestState::IOP)
				{
					// IOP accepts 32-bit integer state, memory and control.
					// EE-specific memory and helpers must never use the IOP layout.
					bool supported = inst.type == ir::Type::Void || inst.type == ir::Type::I32;
					for (u32 arg = 0; arg < ir::ValueOperandCount(inst); ++arg)
						supported &= fn.ValueType(inst.args[arg]) == ir::Type::I32;
					switch (inst.op)
					{
						case ir::Op::Nop: case ir::Op::ConstI32: case ir::Op::Copy:
						case ir::Op::Mul: case ir::Op::MulHiS: case ir::Op::MulHiU:
						case ir::Op::DivS: case ir::Op::DivU: case ir::Op::Msub:
						case ir::Op::Add: case ir::Op::Sub: case ir::Op::And:
						case ir::Op::Or: case ir::Op::Xor: case ir::Op::Not:
						case ir::Op::Shl: case ir::Op::ShrU: case ir::Op::ShrS:
						case ir::Op::CmpLtS: case ir::Op::CmpLtU:
						case ir::Op::CmpEq: case ir::Op::CmpNe: case ir::Op::CmpLeS:
						case ir::Op::CmpGtS: case ir::Op::CmpGeS:
						case ir::Op::Jump: case ir::Op::Branch: case ir::Op::BranchIndirect:
						case ir::Op::ReadCp0: case ir::Op::WriteCp0: case ir::Op::CheckInterrupts:
						case ir::Op::ReadGpr: case ir::Op::WriteGpr:
						case ir::Op::ReadHi: case ir::Op::WriteHi:
						case ir::Op::ReadLo: case ir::Op::WriteLo:
						case ir::Op::Load8S: case ir::Op::Load8U:
						case ir::Op::Load16S: case ir::Op::Load16U: case ir::Op::Load32:
						case ir::Op::Store8: case ir::Op::Store16: case ir::Op::Store32:
						case ir::Op::Resume:
							break;
						default: supported = false; break;
					}
					if (!supported || (inst.aux & ir::IF_WIDE_WRITE))
					{
						if (error) *error = "unsupported IOP IR operation or type";
						return false;
					}
				}
				const bool quad_alu = inst.type == ir::Type::V4U32 &&
					(inst.op == ir::Op::Add || inst.op == ir::Op::Sub || inst.op == ir::Op::And ||
						inst.op == ir::Op::Or || inst.op == ir::Op::Xor || inst.op == ir::Op::Not ||
						inst.op == ir::Op::VShl || inst.op == ir::Op::VShrU || inst.op == ir::Op::VShrS ||
						inst.op == ir::Op::VMinS || inst.op == ir::Op::VMinU || inst.op == ir::Op::VMaxS || inst.op == ir::Op::VMaxU ||
						inst.op == ir::Op::VCmpEq || inst.op == ir::Op::VCmpNe || inst.op == ir::Op::VCmpLtS || inst.op == ir::Op::VCmpLtU ||
						inst.op == ir::Op::VCmpLeS || inst.op == ir::Op::VCmpLeU || inst.op == ir::Op::VShuffle || inst.op == ir::Op::VShuffle2);
				const bool quad_result = inst.type == ir::Type::V4U32 &&
					(inst.op == ir::Op::ConstVec || inst.op == ir::Op::Copy || inst.op == ir::Op::ReadGpr || inst.op == ir::Op::Load128 || quad_alu);
				for (u32 arg = 0; arg < ir::ValueOperandCount(inst); ++arg)
				{
					const ir::Type type = fn.ValueType(inst.args[arg]);
					const bool quad_operand = type == ir::Type::V4U32 &&
						(quad_alu || (inst.op == ir::Op::Copy && quad_result) || (inst.op == ir::Op::Store128 && arg == 1) ||
							(inst.op == ir::Op::WriteGpr && !(inst.aux & ir::IF_WIDE_WRITE)));
					if ((ir::IsVectorType(type) && !quad_operand) ||
						(inst.op == ir::Op::Store128 && arg == 1 && !quad_operand))
					{
						if (error)
							*error = "unsupported IR vector operand";
						return false;
					}
				}
				if ((inst.op == ir::Op::MulHiS || inst.op == ir::Op::MulHiU) &&
					(inst.type != ir::Type::I32 || fn.ValueType(inst.args[0]) != ir::Type::I32 || fn.ValueType(inst.args[1]) != ir::Type::I32))
				{
					if (error) *error = "high multiply requires I32 operands and result";
					return false;
				}
				if ((ir::IsVectorType(inst.type) && !quad_result) || inst.type == ir::Type::Any ||
					(inst.type == ir::Type::I64 && (inst.op == ir::Op::Undef ||
						inst.op == ir::Op::Neg || inst.op == ir::Op::MinS || inst.op == ir::Op::MinU ||
						inst.op == ir::Op::MaxS || inst.op == ir::Op::MaxU || inst.op == ir::Op::DivS ||
						inst.op == ir::Op::DivU || inst.op == ir::Op::RemS || inst.op == ir::Op::RemU)) ||
					((inst.op == ir::Op::ReadGpr || inst.op == ir::Op::WriteGpr) && inst.imm >= 32))
				{
					if (error)
						*error = "unsupported IR operand type or guest register";
					return false;
				}
				switch (inst.op)
				{
					case ir::Op::Jump:
					case ir::Op::Branch:
					case ir::Op::BranchIndirect:
					case ir::Op::Resume:
						break;
					case ir::Op::Trap:
					case ir::Op::CheckEvents:
					case ir::Op::AddCycles:
						if (error)
							*error = "IR exception/event semantics are not modelled";
						return false;
					case ir::Op::Return:
						if (inline_body)
						{
							if (error)
								*error = "inline mode does not support this terminator";
							return false;
						}
						break;
					default:
						if (!IsSupportedOp(inst.op))
						{
							if (error)
								*error = std::string("unsupported IR op ") + ir::OpName(inst.op);
							return false;
						}
						break;
				}
			}
		}
		return true;
	}

	bool LowerBlock(ir::Function& fn, const LowerOptions& options, u8* code, size_t capacity,
		LowerOutput* out, std::string* error)
	{
		*out = {};
		// Optimize a copy so callers can lower the identical input in A/B modes.
		if (options.optimize_ir)
		{
			if (!CanLower(fn, options.inline_body, error, options.guest_state))
				return false;
			ir::Function optimized = fn;
			ForwardGuestStateReads(optimized, options.inline_division, options.forward_quad_state);
			ir::OptimizeIntegerValues(optimized);
			Lowerer lowerer(optimized, options);
			return lowerer.Run(code, capacity, out, error);
		}
		Lowerer lowerer(fn, options);
		return lowerer.Run(code, capacity, out, error);
	}

	bool LowerBlock(ir::Function& fn, u8* code, size_t capacity, LowerOutput* out, std::string* error)
	{
		return LowerBlock(fn, LowerOptions{}, code, capacity, out, error);
	}
} // namespace Arm64Ir

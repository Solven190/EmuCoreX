// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+

#include "arm64/ee/EeIrLower-arm64.h"

#include "arm64/OaknutHelpers-arm64.h"
#include "arm64/cpuRegistersPack-arm64.h"
#include "Memory.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <optional>
#include <vector>

namespace EeIr
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
				case ir::Op::Shl:
				case ir::Op::ShrU:
				case ir::Op::ShrS:
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
				case ir::Op::Store8:
				case ir::Op::Store16:
				case ir::Op::Store32:
				case ir::Op::Store64:
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
		void ForwardEEStateReads(ir::Function& fn)
		{
			struct CachedState { u32 narrow = 0, wide = 0; bool sign_extended = false; };
			for (ir::Block& block : fn.blocks)
			{
				std::array<CachedState, 34> cache{}; // GPR[32], HI, LO
				for (ir::Inst& inst : block.insts)
				{
					const auto kind = ir::Info(inst.op).kind;
					if (kind == ir::OpKind::MemLoad || kind == ir::OpKind::MemStore || kind == ir::OpKind::Helper ||
						inst.op == ir::Op::DivS || inst.op == ir::Op::DivU || inst.op == ir::Op::RemS || inst.op == ir::Op::RemU)
					{
						cache = {};
						continue;
					}
					const bool read = inst.op == ir::Op::ReadGpr || inst.op == ir::Op::ReadHi || inst.op == ir::Op::ReadLo;
					const bool write = inst.op == ir::Op::WriteGpr || inst.op == ir::Op::WriteHi || inst.op == ir::Op::WriteLo;
					if (!read && !write)
						continue;
					const u32 reg = (inst.op == ir::Op::ReadGpr || inst.op == ir::Op::WriteGpr) ?
						static_cast<u32>(inst.imm) : (inst.op == ir::Op::ReadHi || inst.op == ir::Op::WriteHi) ? 32u : 33u;
					if (reg == 0)
						continue;
					CachedState& state = cache[reg];
					if (!ir::IsIntegerType(read ? inst.type : fn.ValueType(inst.args[0])))
					{
						state = {};
						continue;
					}
					if (write)
					{
						const bool wide = reg < 32 ? (inst.aux & ir::IF_WIDE_WRITE) != 0 : fn.ValueType(inst.args[0]) == ir::Type::I64;
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
				, m_inline(options.inline_body)
				, m_hooks(options.hooks)
				, m_capture_exit(options.capture_exit_pc)
				, m_materialize_constants(options.materialize_constants)
				, m_allocate_registers(options.allocate_registers)
			{
			}

			bool Run(u8* code, size_t capacity, LowerOutput* out, std::string* error);

			oak::XReg FrameBase() const { return m_inline ? oak::util::X20 : oak::util::X28; }
			oak::XReg GuestBase() const { return m_inline ? oak::util::X27 : oak::util::X19; }

		private:
			u32 SaveArea() const { return m_inline ? 0u : (m_allocate_registers ? 64u : 32u); }
			static constexpr int kUnallocated = -1;
			void AllocateRegisters();
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

			static s64 GprOffset(u32 reg)
			{
				return static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0])) +
					static_cast<s64>(reg) * static_cast<s64>(sizeof(GPR_reg));
			}

			static s64 LoOffset() { return static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.LO)); }
			static s64 HiOffset() { return static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.HI)); }
			static s64 PcOffset() { return static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.pc)); }

			void EmitFrameAdjust(bool add, u32 amount);
			void EmitPrologue();
			void EmitEntryJump();
			void EmitEpilogue();
			bool EmitInst(const ir::Inst& inst, std::string* error);
			bool Fail(std::string* error, const char* what);

			ir::Function& m_fn;
			std::vector<oak::Label> m_labels;
			u32 m_frame = 0;
			bool m_inline = false;
			const LowerHooks* m_hooks = nullptr;
			bool m_capture_exit = false;
			bool m_materialize_constants = true;
			bool m_allocate_registers = true;
			std::vector<std::optional<u64>> m_constants;
			std::vector<u32> m_slots;
			std::vector<int> m_registers;
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
			if (!CanLower(m_fn, m_inline, error))
				return false;

			const u32 value_count = static_cast<u32>(m_fn.value_types.size());
			m_constants.resize(value_count);
			m_slots.resize(value_count);
			std::vector<bool> defined(value_count, false);
			for (const ir::Block& block : m_fn.blocks)
				for (const ir::Inst& inst : block.insts)
					if (inst.value)
						defined[inst.value] = true;
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
			u32 next_slot = SaveArea();
			for (u32 value = 1; value < value_count; ++value)
			{
				if (m_registers[value] >= 0)
					++out->register_values;
				else if (defined[value] && !m_constants[value])
				{
					++out->spill_values;
					m_slots[value] = next_slot;
					next_slot += 8;
				}
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
					for (const ir::Inst& inst : block.insts)
					{
						if (!EmitInst(inst, error))
							return false;
					}
				}
				out->entry = nullptr;
				out->host_size = static_cast<u32>(oakGetCurrentCodePointer() - start);
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
				for (const ir::Inst& inst : block.insts)
				{
					if (!EmitInst(inst, error))
						return false;
				}
			}

			const u8* const end = oakEndBlock();
			out->entry = start;
			out->host_size = static_cast<u32>(end - start);
			return true;
		}

		bool Lowerer::EmitInst(const ir::Inst& inst, std::string* error)
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

				case ir::Op::Copy:
					if (inst.type == ir::Type::I64)
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
					if (inst.type == ir::Type::I64)
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
					if (inst.type == ir::Type::I64)
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

				case ir::Op::DivS:
				case ir::Op::RemS:
				case ir::Op::DivU:
				case ir::Op::RemU:
				{
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
					const void* helper = nullptr;
					switch (inst.op)
					{
						case ir::Op::Load8U: helper = reinterpret_cast<const void*>(&EeIrMemRead8); break;
						case ir::Op::Load8S: helper = reinterpret_cast<const void*>(&EeIrMemRead8S); break;
						case ir::Op::Load16U: helper = reinterpret_cast<const void*>(&EeIrMemRead16); break;
						case ir::Op::Load16S: helper = reinterpret_cast<const void*>(&EeIrMemRead16S); break;
						default: helper = reinterpret_cast<const void*>(&EeIrMemRead32); break;
					}
					recBeginOaknutEmit();
					load_a();
					if (m_hooks && m_hooks->before_memory)
						m_hooks->before_memory(m_hooks->ctx, inst.guest_pc, (inst.aux & ir::IF_DELAY_SLOT) != 0);
					call_helper(helper);
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
					Store64(inst.value, oak::util::X0);
					recEndOaknutEmit();
					return true;

				case ir::Op::Store8:
				case ir::Op::Store16:
				case ir::Op::Store32:
				case ir::Op::Store64:
				{
					const void* helper = nullptr;
					switch (inst.op)
					{
						case ir::Op::Store8: helper = reinterpret_cast<const void*>(&EeIrMemWrite8); break;
						case ir::Op::Store16: helper = reinterpret_cast<const void*>(&EeIrMemWrite16); break;
						case ir::Op::Store32: helper = reinterpret_cast<const void*>(&EeIrMemWrite32); break;
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
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::ReadGpr:
					recBeginOaknutEmit();
					if (inst.type == ir::Type::I64)
					{
						oakLoad64(Result64(inst.value), {GuestBase(), GprOffset(static_cast<u32>(inst.imm))});
						Store64(inst.value, Result64(inst.value));
					}
					else
					{
						oakLoad32(Result32(inst.value), {GuestBase(), GprOffset(static_cast<u32>(inst.imm))});
						Store32(inst.value, Result32(inst.value));
					}
					recEndOaknutEmit();
					return true;

				case ir::Op::WriteGpr:
					if (inst.imm == 0)
						return true; // $0 is hardwired to zero
					recBeginOaknutEmit();
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

	bool CanLower(const ir::Function& fn, bool inline_body, std::string* error)
	{
		if (!ir::Verify(fn, error))
			return false;
		// Reject before emission: the caller may fall back to the legacy JIT.
		if (fn.value_types.size() > (0x3f00u - 64u) / 8u)
		{
			if (error)
				*error = "IR stack frame too large";
			return false;
		}
		for (const ir::Block& block : fn.blocks)
		{
			for (const ir::Inst& inst : block.insts)
			{
				if (ir::IsVectorType(inst.type) || inst.type == ir::Type::Any ||
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
		out->entry = nullptr;
		out->host_size = 0;
		out->frame_size = 0;
		out->register_values = 0;
		out->spill_values = 0;
		// Optimize a copy so callers can lower the identical input in A/B modes.
		if (options.optimize_ir)
		{
			if (!CanLower(fn, options.inline_body, error))
				return false;
			ir::Function optimized = fn;
			ForwardEEStateReads(optimized);
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
} // namespace EeIr

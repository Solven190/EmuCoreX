// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+

#include "arm64/ee/EeIrLower-arm64.h"

#include "arm64/OaknutHelpers-arm64.h"
#include "arm64/cpuRegistersPack-arm64.h"
#include "Memory.h"

#include <cstdio>
#include <cstring>
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

		class Lowerer
		{
		public:
			Lowerer(ir::Function& fn, const LowerOptions& options)
				: m_fn(fn)
				, m_inline(options.inline_body)
				, m_hooks(options.hooks)
				, m_capture_exit(options.capture_exit_pc)
			{
			}

			bool Run(u8* code, size_t capacity, LowerOutput* out, std::string* error);

			oak::XReg FrameBase() const { return m_inline ? oak::util::X20 : oak::util::X28; }
			oak::XReg GuestBase() const { return m_inline ? oak::util::X27 : oak::util::X19; }

		private:
			static constexpr u32 kSaveArea = 32; // X19/X30 pair + X28 frame pointer

			u32 Slot(u32 value) const { return kSaveArea + (value - 1) * 8; }

			void Load32(u32 value, const oak::WReg& dst)
			{
				oakLoad32(dst, {FrameBase(), static_cast<s64>(Slot(value))});
			}

			void Load64(u32 value, const oak::XReg& dst)
			{
				oakLoad64(dst, {FrameBase(), static_cast<s64>(Slot(value))});
			}

			void Store32(u32 value, const oak::WReg& src)
			{
				oakStore32(src, {FrameBase(), static_cast<s64>(Slot(value))});
			}

			void Store64(u32 value, const oak::XReg& src)
			{
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
			void EmitEpilogue();
			bool EmitInst(const ir::Inst& inst, std::string* error);
			bool Fail(std::string* error, const char* what);

			ir::Function& m_fn;
			std::vector<oak::Label> m_labels;
			u32 m_frame = 0;
			bool m_inline = false;
			const LowerHooks* m_hooks = nullptr;
			bool m_capture_exit = false;
		};

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
				oakAsm->MOV(oak::util::X28, oak::util::SP);
				oakMoveAddressToReg(oak::util::X19, &g_cpuRegistersPack);
			}
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
			m_frame = (kSaveArea + value_count * 8 + 15u) & ~15u;
			if (m_frame > 0x3f00)
				return Fail(error, "stack frame too large");

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
				EmitPrologue();
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
				out->host_size = 0;
				return true;
			}

			oakSetAsmPtr(code, capacity);
			u8* const start = oakStartBlock();

			m_labels.resize(m_fn.blocks.size());
			EmitPrologue();

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
						Load64(a[0], oak::util::X0);
						Store64(inst.value, oak::util::X0);
						recEndOaknutEmit();
					}
					else
					{
						recBeginOaknutEmit();
						load_a();
						store_r();
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
						Load64(a[0], oak::util::X0);
						oakAsm->MVN(oak::util::X0, oak::util::X0);
						Store64(inst.value, oak::util::X0);
					}
					else
					{
						load_a();
						oakAsm->MVN(oak::util::W0, oak::util::W0);
						store_r();
					}
					recEndOaknutEmit();
					return true;
				case ir::Op::Neg:
					recBeginOaknutEmit();
					load_a();
					oakAsm->NEG(oak::util::W0, oak::util::W0);
					store_r();
					recEndOaknutEmit();
					return true;
				case ir::Op::Sext8:
					recBeginOaknutEmit();
					load_a();
					oakAsm->SXTB(oak::util::W0, oak::util::W0);
					store_r();
					recEndOaknutEmit();
					return true;
				case ir::Op::Sext16:
					recBeginOaknutEmit();
					load_a();
					oakAsm->SXTH(oak::util::W0, oak::util::W0);
					store_r();
					recEndOaknutEmit();
					return true;
				case ir::Op::Zext8:
					recBeginOaknutEmit();
					load_a();
					oakAsm->UXTB(oak::util::W0, oak::util::W0);
					store_r();
					recEndOaknutEmit();
					return true;
				case ir::Op::Zext16:
					recBeginOaknutEmit();
					load_a();
					oakAsm->UXTH(oak::util::W0, oak::util::W0);
					store_r();
					recEndOaknutEmit();
					return true;
				case ir::Op::Sext32:
					recBeginOaknutEmit();
					load_a();
					oakAsm->SXTW(oak::util::X0, oak::util::W0);
					Store64(inst.value, oak::util::X0);
					recEndOaknutEmit();
					return true;
				case ir::Op::Zext32:
					recBeginOaknutEmit();
					load_a();
					oakAsm->MOV(oak::util::X0, oak::util::X0);
					Store64(inst.value, oak::util::X0);
					recEndOaknutEmit();
					return true;
				case ir::Op::Trunc32:
					recBeginOaknutEmit();
					Load64(a[0], oak::util::X0);
					Store32(inst.value, oak::util::W0);
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
					const bool wide = (inst.type == ir::Type::I64);
					recBeginOaknutEmit();
					if (wide)
					{
						Load64(a[0], oak::util::X0);
						Load64(a[1], oak::util::X1);
					}
					else
					{
						load_a();
						load_b();
					}

					if (wide)
					{
						switch (inst.op)
						{
							case ir::Op::Add: oakAsm->ADD(oak::util::X0, oak::util::X0, oak::util::X1); break;
							case ir::Op::Sub: oakAsm->SUB(oak::util::X0, oak::util::X0, oak::util::X1); break;
							case ir::Op::And: oakAsm->AND(oak::util::X0, oak::util::X0, oak::util::X1); break;
							case ir::Op::Or: oakAsm->ORR(oak::util::X0, oak::util::X0, oak::util::X1); break;
							case ir::Op::Xor: oakAsm->EOR(oak::util::X0, oak::util::X0, oak::util::X1); break;
							default: return Fail(error, "unsupported 64-bit op");
						}
						Store64(inst.value, oak::util::X0);
					}
					else
					{
						switch (inst.op)
						{
							case ir::Op::Add: oakAsm->ADD(oak::util::W0, oak::util::W0, oak::util::W1); break;
							case ir::Op::Sub: oakAsm->SUB(oak::util::W0, oak::util::W0, oak::util::W1); break;
							case ir::Op::And: oakAsm->AND(oak::util::W0, oak::util::W0, oak::util::W1); break;
							case ir::Op::Or: oakAsm->ORR(oak::util::W0, oak::util::W0, oak::util::W1); break;
							case ir::Op::Xor: oakAsm->EOR(oak::util::W0, oak::util::W0, oak::util::W1); break;
							case ir::Op::MinS:
								oakAsm->CMP(oak::util::W0, oak::util::W1);
								oakAsm->CSEL(oak::util::W0, oak::util::W0, oak::util::W1, oak::Cond::LT);
								break;
							case ir::Op::MinU:
								oakAsm->CMP(oak::util::W0, oak::util::W1);
								oakAsm->CSEL(oak::util::W0, oak::util::W0, oak::util::W1, oak::Cond::LO);
								break;
							case ir::Op::MaxS:
								oakAsm->CMP(oak::util::W0, oak::util::W1);
								oakAsm->CSEL(oak::util::W0, oak::util::W0, oak::util::W1, oak::Cond::GT);
								break;
							case ir::Op::MaxU:
								oakAsm->CMP(oak::util::W0, oak::util::W1);
								oakAsm->CSEL(oak::util::W0, oak::util::W0, oak::util::W1, oak::Cond::HI);
								break;
							default: return Fail(error, "unsupported op");
						}
						store_r();
					}
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::Mul:
					if (inst.type == ir::Type::I64)
					{
						recBeginOaknutEmit();
						Load64(a[0], oak::util::X0);
						Load64(a[1], oak::util::X1);
						oakAsm->MUL(oak::util::X0, oak::util::X0, oak::util::X1);
						Store64(inst.value, oak::util::X0);
						recEndOaknutEmit();
					}
					else
					{
						recBeginOaknutEmit();
						load_a();
						load_b();
						oakAsm->MUL(oak::util::W0, oak::util::W0, oak::util::W1);
						store_r();
						recEndOaknutEmit();
					}
					return true;

				case ir::Op::Shl:
				case ir::Op::ShrU:
				case ir::Op::ShrS:
				{
					const bool wide = (inst.type == ir::Type::I64);
					recBeginOaknutEmit();
					if (wide)
					{
						Load64(a[0], oak::util::X0);
						Load64(a[1], oak::util::X1);
						if (inst.op == ir::Op::Shl)
							oakAsm->LSLV(oak::util::X0, oak::util::X0, oak::util::X1);
						else if (inst.op == ir::Op::ShrU)
							oakAsm->LSRV(oak::util::X0, oak::util::X0, oak::util::X1);
						else
							oakAsm->ASRV(oak::util::X0, oak::util::X0, oak::util::X1);
						Store64(inst.value, oak::util::X0);
					}
					else
					{
						load_a();
						load_b();
						if (inst.op == ir::Op::Shl)
							oakAsm->LSLV(oak::util::W0, oak::util::W0, oak::util::W1);
						else if (inst.op == ir::Op::ShrU)
							oakAsm->LSRV(oak::util::W0, oak::util::W0, oak::util::W1);
						else
							oakAsm->ASRV(oak::util::W0, oak::util::W0, oak::util::W1);
						store_r();
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
						Load64(a[0], oak::util::X0);
						Load64(a[1], oak::util::X1);
						oakAsm->CMP(oak::util::X0, oak::util::X1);
					}
					else
					{
						load_a();
						load_b();
						oakAsm->CMP(oak::util::W0, oak::util::W1);
					}
					oakAsm->CSET(oak::util::W0, cond);
					store_r();
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
						oakLoad64(oak::util::X0, {GuestBase(), GprOffset(static_cast<u32>(inst.imm))});
						Store64(inst.value, oak::util::X0);
					}
					else
					{
						oakLoad32(oak::util::W0, {GuestBase(), GprOffset(static_cast<u32>(inst.imm))});
						store_r();
					}
					recEndOaknutEmit();
					return true;

				case ir::Op::WriteGpr:
					if (inst.imm == 0)
						return true; // $0 is hardwired to zero
					recBeginOaknutEmit();
					if (inst.aux & ir::IF_WIDE_WRITE)
					{
						// Value is already a full 64-bit quantity (jal link).
						Load64(a[0], oak::util::X0);
					}
					else
					{
						load_a();
						oakAsm->SXTW(oak::util::X0, oak::util::W0);
					}
					oakStore64(oak::util::X0, {GuestBase(), GprOffset(static_cast<u32>(inst.imm))});
					recEndOaknutEmit();
					return true;

				case ir::Op::ReadHi:
				case ir::Op::ReadLo:
					recBeginOaknutEmit();
					if (inst.type == ir::Type::I64)
					{
						oakLoad64(oak::util::X0, {GuestBase(), (inst.op == ir::Op::ReadHi) ? HiOffset() : LoOffset()});
						Store64(inst.value, oak::util::X0);
					}
					else
					{
						oakLoad32(oak::util::W0, {GuestBase(), (inst.op == ir::Op::ReadHi) ? HiOffset() : LoOffset()});
						store_r();
					}
					recEndOaknutEmit();
					return true;

				case ir::Op::WriteHi:
				case ir::Op::WriteLo:
					recBeginOaknutEmit();
					if (m_fn.ValueType(a[0]) == ir::Type::I64)
						Load64(a[0], oak::util::X0);
					else
					{
						load_a();
						oakAsm->SXTW(oak::util::X0, oak::util::W0);
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
					load_a();
					oakAsm->CBNZ(oak::util::W0, m_labels[taken - 1]);
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
		if (fn.value_types.size() > (0x3f00u - 32u) / 8u)
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
		Lowerer lowerer(fn, options);
		return lowerer.Run(code, capacity, out, error);
	}

	bool LowerBlock(ir::Function& fn, u8* code, size_t capacity, LowerOutput* out, std::string* error)
	{
		return LowerBlock(fn, LowerOptions{}, code, capacity, out, error);
	}
} // namespace EeIr

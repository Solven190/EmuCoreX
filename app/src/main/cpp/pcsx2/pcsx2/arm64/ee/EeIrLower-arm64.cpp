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

		class Lowerer
		{
		public:
			explicit Lowerer(ir::Function& fn)
				: m_fn(fn)
			{
			}

			bool Run(u8* code, size_t capacity, LowerOutput* out, std::string* error);

		private:
			static constexpr u32 kSaveArea = 32; // X19/X30 pair + X28 frame pointer

			u32 Slot(u32 value) const { return kSaveArea + (value - 1) * 8; }

			void Load32(u32 value, const oak::WReg& dst)
			{
				oakLoad32(dst, {oak::util::X28, static_cast<s64>(Slot(value))});
			}

			void Load64(u32 value, const oak::XReg& dst)
			{
				oakLoad64(dst, {oak::util::X28, static_cast<s64>(Slot(value))});
			}

			void Store32(u32 value, const oak::WReg& src)
			{
				oakStore32(src, {oak::util::X28, static_cast<s64>(Slot(value))});
			}

			void Store64(u32 value, const oak::XReg& src)
			{
				oakStore64(src, {oak::util::X28, static_cast<s64>(Slot(value))});
			}

			static s64 GprOffset(u32 reg)
			{
				return static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0])) +
					static_cast<s64>(reg) * static_cast<s64>(sizeof(GPR_reg));
			}

			static s64 LoOffset() { return static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.LO)); }
			static s64 HiOffset() { return static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.HI)); }
			static s64 PcOffset() { return static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.pc)); }

			void EmitPrologue();
			void EmitEpilogue();
			bool EmitInst(const ir::Inst& inst, std::string* error);
			bool Fail(std::string* error, const char* what);

			ir::Function& m_fn;
			std::vector<oak::Label> m_labels;
			u32 m_frame = 0;
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

		void Lowerer::EmitPrologue()
		{
			recBeginOaknutEmit();
			oakAsm->SUB(oak::util::SP, oak::util::SP, m_frame);
			oakAsm->STP(oak::util::X19, oak::util::X30, oak::util::SP, oak::SOffset<10, 3>(0));
			oakAsm->STP(oak::util::X28, oak::util::XZR, oak::util::SP, oak::SOffset<10, 3>(16));
			oakAsm->MOV(oak::util::X28, oak::util::SP);
			oakMoveAddressToReg(oak::util::X19, &g_cpuRegistersPack);
			recEndOaknutEmit();
		}

		void Lowerer::EmitEpilogue()
		{
			recBeginOaknutEmit();
			oakAsm->LDP(oak::util::X28, oak::util::XZR, oak::util::SP, oak::SOffset<10, 3>(16));
			oakAsm->LDP(oak::util::X19, oak::util::X30, oak::util::SP, oak::SOffset<10, 3>(0));
			oakAsm->ADD(oak::util::SP, oak::util::SP, m_frame);
			oakAsm->RET();
			recEndOaknutEmit();
		}

		bool Lowerer::Run(u8* code, size_t capacity, LowerOutput* out, std::string* error)
		{
			if (!ir::Verify(m_fn, error))
				return false;

			const u32 value_count = static_cast<u32>(m_fn.value_types.size());
			m_frame = (kSaveArea + value_count * 8 + 15u) & ~15u;
			if (m_frame > 0x3f00)
				return Fail(error, "stack frame too large");

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

			auto load_a = [&]() { Load32(a[0], oak::util::W0); };
			auto load_b = [&]() { Load32(a[1], oak::util::W1); };
			auto store_r = [&]() { Store32(inst.value, oak::util::W0); };
			auto call_helper = [&](const void* fn) {
				oakEmitCall(fn);
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
					Load32(a[1], oak::util::W1);
					Load32(a[2], oak::util::W2);
					oakAsm->CMP(oak::util::W0, 0);
					oakAsm->CSEL(oak::util::W0, oak::util::W1, oak::util::W2, oak::Cond::NE);
					store_r();
					recEndOaknutEmit();
					return true;

				case ir::Op::Not:
					recBeginOaknutEmit();
					load_a();
					oakAsm->MVN(oak::util::W0, oak::util::W0);
					store_r();
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
					load_a();
					load_b();
					oakAsm->CMP(oak::util::W0, oak::util::W1);
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
					call_helper(helper);
					store_r();
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::Load64:
					recBeginOaknutEmit();
					load_a();
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
					call_helper(helper);
					recEndOaknutEmit();
					return true;
				}

				case ir::Op::ReadGpr:
					recBeginOaknutEmit();
					oakLoad32(oak::util::W0, {oak::util::X19, GprOffset(static_cast<u32>(inst.imm))});
					store_r();
					recEndOaknutEmit();
					return true;

				case ir::Op::WriteGpr:
					recBeginOaknutEmit();
					load_a();
					oakAsm->SXTW(oak::util::X0, oak::util::W0);
					oakStore64(oak::util::X0, {oak::util::X19, GprOffset(static_cast<u32>(inst.imm))});
					recEndOaknutEmit();
					return true;

				case ir::Op::ReadHi:
				case ir::Op::ReadLo:
					recBeginOaknutEmit();
					oakLoad32(oak::util::W0, {oak::util::X19, (inst.op == ir::Op::ReadHi) ? HiOffset() : LoOffset()});
					store_r();
					recEndOaknutEmit();
					return true;

				case ir::Op::WriteHi:
				case ir::Op::WriteLo:
					recBeginOaknutEmit();
					load_a();
					oakAsm->SXTW(oak::util::X0, oak::util::W0);
					oakStore64(oak::util::X0, {oak::util::X19, (inst.op == ir::Op::WriteHi) ? HiOffset() : LoOffset()});
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
					recBeginOaknutEmit();
					load_a();
					oakStore32(oak::util::W0, {oak::util::X19, PcOffset()});
					EmitEpilogue();
					recEndOaknutEmit();
					return true;

				case ir::Op::Resume:
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

	bool LowerBlock(ir::Function& fn, u8* code, size_t capacity, LowerOutput* out, std::string* error)
	{
		out->entry = nullptr;
		out->host_size = 0;
		Lowerer lowerer(fn);
		return lowerer.Run(code, capacity, out, error);
	}
} // namespace EeIr

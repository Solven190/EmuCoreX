// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+

#include "arm64/ee/EeIrLifter-arm64.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace EeIr
{
	namespace
	{
		inline u32 Op(u32 w) { return w >> 26; }
		inline u32 Rs(u32 w) { return (w >> 21) & 0x1f; }
		inline u32 Rt(u32 w) { return (w >> 16) & 0x1f; }
		inline u32 Rd(u32 w) { return (w >> 11) & 0x1f; }
		inline u32 Sa(u32 w) { return (w >> 6) & 0x1f; }
		inline u32 Funct(u32 w) { return w & 0x3f; }
		inline s32 Simm(u32 w) { return static_cast<s16>(w); }
		inline u32 Uimm(u32 w) { return w & 0xffff; }

		enum class Kind
		{
			Plain,
			Branch,       // conditional, delay slot executes on both paths
			BranchLikely, // conditional, delay slot only executes when taken
			Jump,         // j / jal
			JumpIndirect, // jr / jalr
			Trap,         // syscall / break
			Unsupported,
		};

		class Lifter
		{
		public:
			Lifter(const u32* code, const LiftOptions& options, ir::Function& fn)
				: m_code(code)
				, m_opts(options)
				, m_fn(fn)
				, m_b(fn)
			{
			}

			bool Run(u32* end_pc, std::string* error);

		private:
			u32 ReadGpr(u32 index) { return m_b.Emit(ir::Op::ReadGpr, ir::Type::I32, {}, index); }
			void WriteGpr(u32 index, u32 value) { m_b.Emit(ir::Op::WriteGpr, ir::Type::Void, {value}, index); }
			u32 Const(u32 value) { return m_b.ConstI32(value); }
			u32 Bin(ir::Op op, u32 a, u32 b) { return m_b.Emit2(op, ir::Type::I32, a, b); }
			u32 Un(ir::Op op, u32 a) { return m_b.Emit1(op, ir::Type::I32, a); }

			bool Fail(std::string* error, const char* what, u32 pc)
			{
				if (error)
				{
					char buffer[96];
					std::snprintf(buffer, sizeof(buffer), "%s at 0x%08x", what, pc);
					*error = buffer;
				}
				return false;
			}

			Kind Classify(u32 word) const;
			u32 Addr(u32 rs, s32 offset);
			bool LiftPlain(u32 word, u32 pc, std::string* error);
			bool LiftSpecial(u32 word, u32 pc, std::string* error);
			bool LiftRegimmBranch(u32 word, u32 pc, u32* cond, bool* link, std::string* error);

			// Lifts the delay slot into the current block. Fails when the slot
			// is itself a control instruction (not modelled yet).
			bool LiftDelaySlot(u32 delay_pc, std::string* error);

			const u32* m_code;
			LiftOptions m_opts;
			ir::Function& m_fn;
			ir::Builder m_b;
		};

		Kind Lifter::Classify(u32 word) const
		{
			const u32 op = Op(word);
			switch (op)
			{
				case 0x00:
				{
					switch (Funct(word))
					{
						case 0x08:
						case 0x09:
							return Kind::JumpIndirect;
						case 0x0C:
						case 0x0D:
							return Kind::Trap;
						default:
							return Kind::Plain;
					}
				}
				case 0x01:
				{
					const u32 rt = Rt(word);
					if (rt <= 0x03 || (rt >= 0x10 && rt <= 0x13))
						return Kind::Branch;
					return Kind::Unsupported;
				}
				case 0x02:
				case 0x03:
					return Kind::Jump;
				case 0x04:
				case 0x05:
				case 0x06:
				case 0x07:
					return Kind::Branch;
				case 0x14:
				case 0x15:
				case 0x16:
				case 0x17:
					return Kind::BranchLikely;
				default:
					return Kind::Plain;
			}
		}

		bool Lifter::LiftSpecial(u32 word, u32 pc, std::string* error)
		{
			const u32 rs = Rs(word);
			const u32 rt = Rt(word);
			const u32 rd = Rd(word);
			const u32 sa = Sa(word);

			switch (Funct(word))
			{
				case 0x00: // sll
					WriteGpr(rd, Bin(ir::Op::Shl, ReadGpr(rt), Const(sa)));
					return true;
				case 0x02: // srl
					WriteGpr(rd, Bin(ir::Op::ShrU, ReadGpr(rt), Const(sa)));
					return true;
				case 0x03: // sra
					WriteGpr(rd, Bin(ir::Op::ShrS, ReadGpr(rt), Const(sa)));
					return true;
				case 0x04: // sllv
					WriteGpr(rd, Bin(ir::Op::Shl, ReadGpr(rt), ReadGpr(rs)));
					return true;
				case 0x06: // srlv
					WriteGpr(rd, Bin(ir::Op::ShrU, ReadGpr(rt), ReadGpr(rs)));
					return true;
				case 0x07: // srav
					WriteGpr(rd, Bin(ir::Op::ShrS, ReadGpr(rt), ReadGpr(rs)));
					return true;
				case 0x0A: // movz
				{
					const u32 value = ReadGpr(rs);
					const u32 old = ReadGpr(rd);
					const u32 cond = Bin(ir::Op::CmpEq, ReadGpr(rt), Const(0));
					WriteGpr(rd, m_b.Emit3(ir::Op::Select, ir::Type::I32, cond, value, old));
					return true;
				}
				case 0x0B: // movn
				{
					const u32 value = ReadGpr(rs);
					const u32 old = ReadGpr(rd);
					const u32 cond = Bin(ir::Op::CmpNe, ReadGpr(rt), Const(0));
					WriteGpr(rd, m_b.Emit3(ir::Op::Select, ir::Type::I32, cond, value, old));
					return true;
				}
				case 0x0F: // sync
					m_b.Emit0(ir::Op::Nop);
					return true;
				case 0x10: // mfhi
					WriteGpr(rd, m_b.Emit0(ir::Op::ReadHi, ir::Type::I32));
					return true;
				case 0x11: // mthi
					m_b.Emit1(ir::Op::WriteHi, ir::Type::Void, ReadGpr(rs));
					return true;
				case 0x12: // mflo
					WriteGpr(rd, m_b.Emit0(ir::Op::ReadLo, ir::Type::I32));
					return true;
				case 0x13: // mtlo
					m_b.Emit1(ir::Op::WriteLo, ir::Type::Void, ReadGpr(rs));
					return true;
				case 0x18: // mult
				case 0x19: // multu
				{
					const bool sign = (Funct(word) == 0x18);
					const u32 a = m_b.Emit1(sign ? ir::Op::Sext32 : ir::Op::Zext32, ir::Type::I64, ReadGpr(rs));
					const u32 b = m_b.Emit1(sign ? ir::Op::Sext32 : ir::Op::Zext32, ir::Type::I64, ReadGpr(rt));
					const u32 product = m_b.Emit2(ir::Op::Mul, ir::Type::I64, a, b);
					const u32 lo = m_b.Emit1(ir::Op::Trunc32, ir::Type::I32, product);
					const u32 hi64 = m_b.Emit2(ir::Op::ShrS, ir::Type::I64, product, m_b.ConstI64(32));
					const u32 hi = m_b.Emit1(ir::Op::Trunc32, ir::Type::I32, hi64);
					m_b.Emit1(ir::Op::WriteLo, ir::Type::Void, lo);
					m_b.Emit1(ir::Op::WriteHi, ir::Type::Void, hi);
					return true;
				}
				case 0x1A: // div
				case 0x1B: // divu
				{
					const bool sign = (Funct(word) == 0x1A);
					const u32 a = ReadGpr(rs);
					const u32 b = ReadGpr(rt);
					m_b.Emit1(ir::Op::WriteLo, ir::Type::Void, Bin(sign ? ir::Op::DivS : ir::Op::DivU, a, b));
					m_b.Emit1(ir::Op::WriteHi, ir::Type::Void, Bin(sign ? ir::Op::RemS : ir::Op::RemU, a, b));
					return true;
				}
				case 0x20: // add
				case 0x21: // addu
					WriteGpr(rd, Bin(ir::Op::Add, ReadGpr(rs), ReadGpr(rt)));
					return true;
				case 0x22: // sub
				case 0x23: // subu
					WriteGpr(rd, Bin(ir::Op::Sub, ReadGpr(rs), ReadGpr(rt)));
					return true;
				case 0x24: // and
					WriteGpr(rd, Bin(ir::Op::And, ReadGpr(rs), ReadGpr(rt)));
					return true;
				case 0x25: // or
					WriteGpr(rd, Bin(ir::Op::Or, ReadGpr(rs), ReadGpr(rt)));
					return true;
				case 0x26: // xor
					WriteGpr(rd, Bin(ir::Op::Xor, ReadGpr(rs), ReadGpr(rt)));
					return true;
				case 0x27: // nor
					WriteGpr(rd, Un(ir::Op::Not, Bin(ir::Op::Or, ReadGpr(rs), ReadGpr(rt))));
					return true;
				case 0x2A: // slt
					WriteGpr(rd, Bin(ir::Op::CmpLtS, ReadGpr(rs), ReadGpr(rt)));
					return true;
				case 0x2B: // sltu
					WriteGpr(rd, Bin(ir::Op::CmpLtU, ReadGpr(rs), ReadGpr(rt)));
					return true;
				default:
					return Fail(error, "unsupported SPECIAL instruction", pc);
			}
		}

		bool Lifter::LiftRegimmBranch(u32 word, u32 pc, u32* cond, bool* link, std::string* error)
		{
			const u32 rs = Rs(word);
			const u32 value = ReadGpr(rs);
			switch (Rt(word))
			{
				case 0x00: // bltz
				case 0x02: // bltzl
				case 0x10: // bltzal
				case 0x12: // bltzall
					*cond = Bin(ir::Op::CmpLtS, value, Const(0));
					*link = (Rt(word) >= 0x10);
					return true;
				case 0x01: // bgez
				case 0x03: // bgezl
				case 0x11: // bgezal
				case 0x13: // bgezall
					*cond = Bin(ir::Op::CmpGeS, value, Const(0));
					*link = (Rt(word) >= 0x10);
					return true;
				default:
					return Fail(error, "unsupported REGIMM instruction", pc);
			}
		}

		bool Lifter::LiftPlain(u32 word, u32 pc, std::string* error)
		{
			if (word == 0)
			{
				m_b.Emit0(ir::Op::Nop);
				return true;
			}

			const u32 op = Op(word);
			if (op == 0x00)
				return LiftSpecial(word, pc, error);

			const u32 rs = Rs(word);
			const u32 rt = Rt(word);
			const s32 simm = Simm(word);
			const u32 uimm = Uimm(word);

			switch (op)
			{
				case 0x08: // addi
				case 0x09: // addiu
					WriteGpr(rt, Bin(ir::Op::Add, ReadGpr(rs), Const(static_cast<u32>(simm))));
					return true;
				case 0x0A: // slti
					WriteGpr(rt, Bin(ir::Op::CmpLtS, ReadGpr(rs), Const(static_cast<u32>(simm))));
					return true;
				case 0x0B: // sltiu
					WriteGpr(rt, Bin(ir::Op::CmpLtU, ReadGpr(rs), Const(static_cast<u32>(simm))));
					return true;
				case 0x0C: // andi
					WriteGpr(rt, Bin(ir::Op::And, ReadGpr(rs), Const(uimm)));
					return true;
				case 0x0D: // ori
					WriteGpr(rt, Bin(ir::Op::Or, ReadGpr(rs), Const(uimm)));
					return true;
				case 0x0E: // xori
					WriteGpr(rt, Bin(ir::Op::Xor, ReadGpr(rs), Const(uimm)));
					return true;
				case 0x0F: // lui
					WriteGpr(rt, Const(uimm << 16));
					return true;
				case 0x20: // lb
					WriteGpr(rt, m_b.Emit1(ir::Op::Load8S, ir::Type::I32, Addr(rs, simm)));
					return true;
				case 0x21: // lh
					WriteGpr(rt, m_b.Emit1(ir::Op::Load16S, ir::Type::I32, Addr(rs, simm)));
					return true;
				case 0x23: // lw
					WriteGpr(rt, m_b.Emit1(ir::Op::Load32, ir::Type::I32, Addr(rs, simm)));
					return true;
				case 0x24: // lbu
					WriteGpr(rt, m_b.Emit1(ir::Op::Load8U, ir::Type::I32, Addr(rs, simm)));
					return true;
				case 0x25: // lhu
					WriteGpr(rt, m_b.Emit1(ir::Op::Load16U, ir::Type::I32, Addr(rs, simm)));
					return true;
				case 0x28: // sb
					m_b.Emit2(ir::Op::Store8, ir::Type::Void, Addr(rs, simm), ReadGpr(rt));
					return true;
				case 0x29: // sh
					m_b.Emit2(ir::Op::Store16, ir::Type::Void, Addr(rs, simm), ReadGpr(rt));
					return true;
				case 0x2B: // sw
					m_b.Emit2(ir::Op::Store32, ir::Type::Void, Addr(rs, simm), ReadGpr(rt));
					return true;
				default:
					return Fail(error, "unsupported instruction", pc);
			}
		}

		u32 Lifter::Addr(u32 rs, s32 offset)
		{
			if (offset == 0)
				return ReadGpr(rs);
			return Bin(ir::Op::Add, ReadGpr(rs), Const(static_cast<u32>(offset)));
		}

		bool Lifter::LiftDelaySlot(u32 delay_pc, std::string* error)
		{
			const u32 word = m_code[(delay_pc - m_opts.start_pc) >> 2];
			if (Classify(word) != Kind::Plain)
				return Fail(error, "control instruction in a delay slot", delay_pc);

			m_b.SetGuestPc(delay_pc);
			return LiftPlain(word, delay_pc, error);
		}

		bool Lifter::Run(u32* end_pc, std::string* error)
		{
			const u32 start = m_opts.start_pc;
			const u32 page_end = (start & ~0xfffu) + 0x1000u;
			const u32 limit = std::min(start + m_opts.max_insts * 4u, page_end);

			m_b.CreateBlock(start);
			m_b.SetGuestPc(start);

			u32 pc = start;
			while (pc < limit)
			{
				const u32 word = m_code[(pc - start) >> 2];
				const Kind kind = Classify(word);

				if (kind == Kind::Plain)
				{
					if (!LiftPlain(word, pc, error))
						return false;
					pc += 4;
					continue;
				}

				if (kind == Kind::Unsupported)
					return Fail(error, "unsupported instruction", pc);

				if (kind == Kind::Trap)
				{
					m_b.SetGuestPc(pc);
					m_b.Trap();
					m_b.SetBlockEnd(pc + 4);
					*end_pc = pc + 4;
					return true;
				}

				// Control flow: every path needs its delay slot.
				const u32 delay_pc = pc + 4;
				if (delay_pc >= limit)
					return Fail(error, "delay slot crosses the block limit", pc);

				const u32 branch_pc = pc;
				const u32 fallthrough_pc = delay_pc + 4;
				u32 cond = 0;
				bool link = false;
				u32 guest_target = 0;

				switch (kind)
				{
					case Kind::Branch:
					case Kind::BranchLikely:
					{
						switch (Op(word))
						{
							case 0x01:
								if (!LiftRegimmBranch(word, pc, &cond, &link, error))
									return false;
								break;
							case 0x04: // beq
							case 0x14: // beql
								cond = Bin(ir::Op::CmpEq, ReadGpr(Rs(word)), ReadGpr(Rt(word)));
								break;
							case 0x05: // bne
							case 0x15: // bnel
								cond = Bin(ir::Op::CmpNe, ReadGpr(Rs(word)), ReadGpr(Rt(word)));
								break;
							case 0x06: // blez
							case 0x16: // blezl
								cond = Bin(ir::Op::CmpLeS, ReadGpr(Rs(word)), Const(0));
								break;
							case 0x07: // bgtz
							case 0x17: // bgtzl
								cond = Bin(ir::Op::CmpGtS, ReadGpr(Rs(word)), Const(0));
								break;
							default:
								return Fail(error, "unsupported branch", pc);
						}
						guest_target = static_cast<u32>(static_cast<s32>(delay_pc) + Simm(word) * 4);
						break;
					}
					case Kind::Jump:
						guest_target = ((delay_pc & 0xf0000000u) | ((word & 0x03ffffffu) << 2));
						link = (Op(word) == 0x03);
						break;
					case Kind::JumpIndirect:
					{
						const u32 target = (Funct(word) == 0x09) ? ReadGpr(Rs(word)) : ReadGpr(Rs(word));
						link = (Funct(word) == 0x09);
						if (link)
						{
							// jalr writes rd (defaults to 31 when rd == 0).
							const u32 rd = Rd(word);
							WriteGpr(rd == 0 ? 31 : rd, Const(branch_pc + 8));
						}
						if (!LiftDelaySlot(delay_pc, error))
							return false;
						m_b.SetGuestPc(fallthrough_pc);
						m_b.BranchIndirect(target);
						m_b.SetBlockEnd(fallthrough_pc);
						*end_pc = fallthrough_pc;
						return true;
					}
					default:
						return Fail(error, "unsupported control flow", pc);
				}

				if (link)
					WriteGpr(31, Const(branch_pc + 8));

				const u32 original_block = m_b.CurrentBlock();

				if (kind == Kind::BranchLikely)
				{
					// The delay slot only runs on the taken path, so both
					// outcomes become separate continuation blocks.
					const u32 taken = m_b.CreateBlock(guest_target);
					const u32 skipped = m_b.CreateBlock(fallthrough_pc);

					m_b.SetBlock(original_block);
					m_b.SetGuestPc(branch_pc);
					m_b.Branch(cond, taken, skipped, guest_target);

					m_b.SetBlock(taken);
					m_b.SetGuestPc(delay_pc);
					if (!LiftDelaySlot(delay_pc, error))
						return false;
					m_b.SetGuestPc(fallthrough_pc);
					m_b.Resume(guest_target);
					m_b.SetBlockEnd(fallthrough_pc);

					m_b.SetBlock(skipped);
					m_b.SetGuestPc(fallthrough_pc);
					m_b.Resume(fallthrough_pc);
					m_b.SetBlockEnd(fallthrough_pc);

					*end_pc = fallthrough_pc;
					return true;
				}

				// Normal branches and jumps execute the delay slot on every path.
				if (!LiftDelaySlot(delay_pc, error))
					return false;

				if (kind == Kind::Branch)
				{
					const u32 taken = m_b.CreateBlock(guest_target);
					const u32 fallthrough_block = m_b.CreateBlock(fallthrough_pc);

					// The branch itself belongs to the original block.
					m_b.SetBlock(original_block);
					m_b.SetGuestPc(branch_pc);
					m_b.Branch(cond, taken, fallthrough_block, guest_target);
					m_b.SetBlockEnd(fallthrough_pc);

					m_b.SetBlock(taken);
					m_b.SetGuestPc(guest_target);
					m_b.Resume(guest_target);
					m_b.SetBlockEnd(guest_target);

					m_b.SetBlock(fallthrough_block);
					m_b.SetGuestPc(fallthrough_pc);
					m_b.Resume(fallthrough_pc);
					m_b.SetBlockEnd(fallthrough_pc);
				}
				else // Kind::Jump
				{
					m_b.SetGuestPc(fallthrough_pc);
					m_b.Resume(guest_target);
					m_b.SetBlockEnd(fallthrough_pc);
				}

				*end_pc = fallthrough_pc;
				return true;
			}

			// Fell off the decode limit: this is a fall-through block.
			m_b.SetGuestPc(pc);
			m_b.Resume(pc);
			m_b.SetBlockEnd(pc);
			*end_pc = pc;
			return true;
		}
	} // namespace

	bool LiftBlock(const u32* code, const LiftOptions& options, ir::Function& out, u32* end_pc, std::string* error)
	{
		out = ir::Function();
		out.guest_entry = options.start_pc;

		Lifter lifter(code, options, out);
		return lifter.Run(end_pc, error);
	}

#if defined(EMUCOREX_ENABLE_NATIVE_SELF_TESTS)
	bool RunSelfTests()
	{
		// Fall-through, conditional branch and delay slot.
		{
			alignas(4) static const u32 code[] = {
				0x24080005, // addiu t0, zero, 5
				0x11000002, // beq t0, zero, +2
				0x25080001, // addiu t0, t0, 1 (delay slot)
				0x00000000, // nop
			};
			ir::Function fn;
			u32 end = 0;
			std::string error;
			if (!LiftBlock(code, {0x00100000, 4}, fn, &end, &error))
			{
				std::printf("EE IR case 1 lift failed: %s\n", error.c_str());
				return false;
			}
			if (end != 0x0010000c || !ir::Verify(fn, &error))
			{
				std::printf("EE IR case 1 failed: end=0x%08x error=%s\n", end, error.c_str());
				return false;
			}
			const std::string dump = ir::Dump(fn);
			if (dump.find("CmpEq") == std::string::npos || dump.find("Branch") == std::string::npos ||
				dump.find("Resume") == std::string::npos || dump.find("Add") == std::string::npos)
			{
				std::printf("EE IR case 1 dump mismatch\n%s\n", dump.c_str());
				return false;
			}
		}

		// jal writes the link register and exits at the target.
		{
			alignas(4) static const u32 code[] = {
				0x0c040800, // jal 0x00102000
				0x00000000, // nop
			};
			ir::Function fn;
			u32 end = 0;
			std::string error;
			if (!LiftBlock(code, {0x00100000, 2}, fn, &end, &error))
				return false;
			if (end != 0x00100008 || !ir::Verify(fn, &error))
				return false;
			const std::string dump = ir::Dump(fn);
			if (dump.find("WriteGpr") == std::string::npos || dump.find("0x00100008") == std::string::npos ||
				dump.find("0x00102000") == std::string::npos)
				return false;
		}

		// jr becomes an indirect terminator.
		{
			alignas(4) static const u32 code[] = {
				0x03e00008, // jr $31
				0x00000000, // nop
			};
			ir::Function fn;
			u32 end = 0;
			std::string error;
			if (!LiftBlock(code, {0x00100000, 2}, fn, &end, &error))
				return false;
			if (end != 0x00100008 || !ir::Verify(fn, &error))
				return false;
			if (ir::Dump(fn).find("BranchIndirect") == std::string::npos)
				return false;
		}

		// mult/div produce HI/LO state updates.
		{
			alignas(4) static const u32 code[] = {
				0x01090018, // mult t0, t1
				0x00005012, // mflo t2
			};
			ir::Function fn;
			u32 end = 0;
			std::string error;
			if (!LiftBlock(code, {0x00100000, 2}, fn, &end, &error))
				return false;
			if (!ir::Verify(fn, &error))
				return false;
			const std::string dump = ir::Dump(fn);
			if (dump.find("Mul") == std::string::npos || dump.find("Sext32") == std::string::npos ||
				dump.find("WriteLo") == std::string::npos || dump.find("ReadLo") == std::string::npos)
				return false;
		}

		// Likely branch: delay slot only on the taken path.
		{
			alignas(4) static const u32 code[] = {
				0x51000001, // beql t0, zero, +1
				0x25080001, // addiu t0, t0, 1 (conditional delay slot)
				0x00000000, // nop
			};
			ir::Function fn;
			u32 end = 0;
			std::string error;
			if (!LiftBlock(code, {0x00100000, 3}, fn, &end, &error))
				return false;
			if (end != 0x00100008 || !ir::Verify(fn, &error))
				return false;
			if (fn.blocks.size() != 3)
				return false;
		}

		// Unsupported instructions must fail so the caller can fall back.
		{
			alignas(4) static const u32 code[] = {
				0x89280000, // lwl t0, 0(t1)
			};
			ir::Function fn;
			u32 end = 0;
			std::string error;
			if (LiftBlock(code, {0x00100000, 1}, fn, &end, &error))
				return false;
		}

		return true;
	}
#endif
} // namespace EeIr

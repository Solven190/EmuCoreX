// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+
#include "arm64/iop/IopIrLifter-arm64.h"
#include <algorithm>

namespace IopIr
{
    namespace
    {
        bool LiftMemory(ir::Builder& b, u32 word)
        {
            const u32 op = word >> 26, rs = (word >> 21) & 31, rt = (word >> 16) & 31;
            const auto read = [&](u32 r) { return r == 0 ? b.ConstI32(0) : b.Emit(ir::Op::ReadGpr, ir::Type::I32, {}, r); };
            const auto bin = [&](ir::Op operation, u32 a, u32 c) { return b.Emit2(operation, ir::Type::I32, a, c); };
            const auto write = [&](u32 v) { b.Emit1(ir::Op::WriteGpr, ir::Type::Void, v, rt); };
            const u32 address = bin(ir::Op::Add, read(rs), b.ConstI32(static_cast<u32>(static_cast<s16>(word))));
            if (op == 32 || op == 33 || op == 35 || op == 36 || op == 37)
            {
                const ir::Op load = op == 32 ? ir::Op::Load8S : op == 33 ? ir::Op::Load16S :
                    op == 36 ? ir::Op::Load8U : op == 37 ? ir::Op::Load16U : ir::Op::Load32;
                write(b.Emit1(load, ir::Type::I32, address)); // keep loads into $0 for MMIO side effects
                return true;
            }
            if (op == 40 || op == 41 || op == 43)
            {
                b.Emit2(op == 40 ? ir::Op::Store8 : op == 41 ? ir::Op::Store16 : ir::Op::Store32,
                    ir::Type::Void, address, read(rt));
                return true;
            }
            // R3000A little-endian word merges. Capture the original address
            // before the read, including base==target. Each shift is 0..24.
            const u32 shift = bin(ir::Op::Shl, bin(ir::Op::And, address, b.ConstI32(3)), b.ConstI32(3));
            const u32 complement = bin(ir::Op::Sub, b.ConstI32(24), shift);
            const u32 aligned = bin(ir::Op::And, address, b.ConstI32(0xfffffffcu));
            const u32 memory = b.Emit1(ir::Op::Load32, ir::Type::I32, aligned);
            if ((op == 34 || op == 38) && rt == 0) return true;
            const u32 reg = read(rt);
            u32 merged;
            if (op == 34) // LWL
                merged = bin(ir::Op::Or, bin(ir::Op::And, reg, bin(ir::Op::ShrU, b.ConstI32(0x00ffffff), shift)),
                    bin(ir::Op::Shl, memory, complement));
            else if (op == 38) // LWR
                merged = bin(ir::Op::Or, bin(ir::Op::And, reg, bin(ir::Op::Shl, b.ConstI32(0xffffff00), complement)),
                    bin(ir::Op::ShrU, memory, shift));
            else if (op == 42) // SWL
                merged = bin(ir::Op::Or, bin(ir::Op::ShrU, reg, complement),
                    bin(ir::Op::And, memory, bin(ir::Op::Shl, b.ConstI32(0xffffff00), shift)));
            else // SWR
                merged = bin(ir::Op::Or, bin(ir::Op::Shl, reg, shift),
                    bin(ir::Op::And, memory, bin(ir::Op::ShrU, b.ConstI32(0x00ffffff), complement)));
            if (op == 34 || op == 38) write(merged);
            else b.Emit2(ir::Op::Store32, ir::Type::Void, aligned, merged);
            return true;
        }

        bool LiftInstruction(ir::Builder& b, u32 word)
        {
            const u32 rs = (word >> 21) & 31, rt = (word >> 16) & 31;
            const u32 rd = (word >> 11) & 31, sa = (word >> 6) & 31;
            const auto read = [&](u32 r) { return r == 0 ? b.ConstI32(0) : b.Emit(ir::Op::ReadGpr, ir::Type::I32, {}, r); };
            const auto write = [&](u32 r, u32 v) { b.Emit1(ir::Op::WriteGpr, ir::Type::Void, v, r); };
            const u32 opcode = word >> 26;
            if ((opcode >= 32 && opcode <= 38) || (opcode >= 40 && opcode <= 43) || opcode == 46)
                return LiftMemory(b, word);
            ir::Op op;
            switch (opcode)
            {
                case 9: op = ir::Op::Add; break; // ADDIU: modulo 32 bits
                case 10: op = ir::Op::CmpLtS; break;
                case 11: op = ir::Op::CmpLtU; break;
                case 12: op = ir::Op::And; break;
                case 13: op = ir::Op::Or; break;
                case 14: op = ir::Op::Xor; break;
                case 15: write(rt, b.ConstI32(word << 16)); return true;
                case 0:
                {
                    switch (word & 63)
                    {
                        case 0: op = ir::Op::Shl; break;
                        case 2: op = ir::Op::ShrU; break;
                        case 3: op = ir::Op::ShrS; break;
                        case 4: op = ir::Op::Shl; break;
                        case 6: op = ir::Op::ShrU; break;
                        case 7: op = ir::Op::ShrS; break;
                        case 0x10: write(rd, b.Emit0(ir::Op::ReadHi, ir::Type::I32)); return true;
                        case 0x12: write(rd, b.Emit0(ir::Op::ReadLo, ir::Type::I32)); return true;
                        case 0x11: b.Emit1(ir::Op::WriteHi, ir::Type::Void, read(rs)); return true;
                        case 0x13: b.Emit1(ir::Op::WriteLo, ir::Type::Void, read(rs)); return true;
                        case 0x21: op = ir::Op::Add; break;
                        case 0x23: op = ir::Op::Sub; break;
                        case 0x24: op = ir::Op::And; break;
                        case 0x25: op = ir::Op::Or; break;
                        case 0x26: case 0x27: op = ir::Op::Xor; break;
                        case 0x2a: op = ir::Op::CmpLtS; break;
                        case 0x2b: op = ir::Op::CmpLtU; break;
                        default: return false;
                    }
                    const u32 funct = word & 63;
                    u32 value;
                    if (funct <= 7)
                    {
                        const u32 amount = funct <= 3 ? b.ConstI32(sa) : read(rs);
                        value = b.Emit2(op, ir::Type::I32, read(rt), amount);
                    }
                    else if (funct == 0x27)
                    {
                        value = b.Emit2(ir::Op::Or, ir::Type::I32, read(rs), read(rt));
                        value = b.Emit1(ir::Op::Not, ir::Type::I32, value);
                    }
                    else
                        value = b.Emit2(op, ir::Type::I32, read(rs), read(rt));
                    write(rd, value);
                    return true;
                }
                default: return false;
            }
            const u32 immediate = (word >> 26) <= 11 ? static_cast<u32>(static_cast<s16>(word)) : word & 0xffff;
            write(rt, b.Emit2(op, ir::Type::I32, read(rs), b.ConstI32(immediate)));
            return true;
        }
    }

    bool LiftSequence(const u32* code, u32 start_pc, u32 max_insts,
        ir::Function& out, u32* accepted, std::string* error)
    {
        out = {};
        *accepted = 0;
        if (!code || (start_pc & 3) || max_insts == 0)
        {
            if (error) *error = "empty IOP sequence or misaligned PC";
            return false;
        }
        ir::Builder b(out);
        b.CreateBlock(start_pc);
        const u32 limit = std::min(max_insts, (0x1000u - (start_pc & 0xfff)) / 4);
        for (u32 i = 0; i < limit; ++i)
        {
            b.SetGuestPc(start_pc + i * 4);
            if (!LiftInstruction(b, code[i])) break;
            ++*accepted;
        }
        if (*accepted == 0)
        {
            out = {};
            if (error) *error = "unsupported IOP instruction at sequence start";
            return false;
        }
        const u32 end = start_pc + *accepted * 4;
        b.SetBlockEnd(end);
        b.Resume(end);
        return ir::Verify(out, error);
    }
}

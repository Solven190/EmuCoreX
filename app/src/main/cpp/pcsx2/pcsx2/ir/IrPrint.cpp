// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+

#include "ir/Ir.h"

#include <cstdio>
#include <cstring>

namespace ir
{
	namespace
	{
		void AppendValue(std::string& out, const Function& fn, u32 id)
		{
			if (id == 0)
			{
				out += "-";
				return;
			}

			char buffer[32];
			std::snprintf(buffer, sizeof(buffer), "v%u", id);
			out += buffer;
		}

		void AppendImmediate(std::string& out, const Inst& inst)
		{
			char buffer[64];
			switch (inst.op)
			{
				case Op::ConstI32:
					std::snprintf(buffer, sizeof(buffer), "0x%08x", static_cast<u32>(inst.imm));
					out += buffer;
					return;
				case Op::ConstI64:
					std::snprintf(buffer, sizeof(buffer), "0x%016llx", static_cast<unsigned long long>(inst.imm));
					out += buffer;
					return;
				case Op::ConstF32:
				{
					u32 bits = static_cast<u32>(inst.imm);
					float value;
					std::memcpy(&value, &bits, sizeof(value));
					std::snprintf(buffer, sizeof(buffer), "%g", static_cast<double>(value));
					out += buffer;
					return;
				}
				case Op::ConstVec:
					std::snprintf(buffer, sizeof(buffer), "vec%u", static_cast<u32>(inst.imm));
					out += buffer;
					return;
				case Op::ReadGpr:
				case Op::WriteGpr:
				case Op::ReadFpr:
				case Op::WriteFpr:
				case Op::ReadVf:
				case Op::WriteVf:
				case Op::ReadVi:
				case Op::WriteVi:
				case Op::ReadAcc:
				case Op::WriteAcc:
					std::snprintf(buffer, sizeof(buffer), "#%u", static_cast<u32>(inst.imm));
					out += buffer;
					return;
				case Op::VBroadcast:
					std::snprintf(buffer, sizeof(buffer), "lane=%u", static_cast<u32>(inst.imm));
					out += buffer;
					return;
				case Op::VShuffle:
				case Op::VShuffle2:
					std::snprintf(buffer, sizeof(buffer), "selectors=0x%03x", static_cast<u32>(inst.imm));
					out += buffer;
					return;
				case Op::VMerge:
				case Op::VMove:
					std::snprintf(buffer, sizeof(buffer), "mask=0x%02x", static_cast<u32>(inst.imm));
					out += buffer;
					return;
				case Op::AddCycles:
					std::snprintf(buffer, sizeof(buffer), "%u", static_cast<u32>(inst.imm));
					out += buffer;
					return;
				case Op::CallHelper:
					std::snprintf(buffer, sizeof(buffer), "helper#%u", static_cast<u32>(inst.imm));
					out += buffer;
					return;
				case Op::Jump:
				case Op::Branch:
				case Op::Resume:
				case Op::BranchIndirect:
					if (inst.imm)
					{
						std::snprintf(buffer, sizeof(buffer), "guest=0x%08x", static_cast<u32>(inst.imm));
						out += buffer;
					}
					return;
				default:
					return;
			}
		}
	} // namespace

	void Dump(const Function& fn, std::string& out)
	{
		char buffer[128];
		std::snprintf(buffer, sizeof(buffer), "func guest=0x%08x blocks=%zu values=%zu entry=b%u\n",
			fn.guest_entry, fn.blocks.size(), fn.value_types.size() - 1, fn.entry);
		out += buffer;

		for (const Block& block : fn.blocks)
		{
			std::snprintf(buffer, sizeof(buffer), "block b%u guest=[0x%08x,0x%08x)",
				block.id, block.guest_start, block.guest_end);
			out += buffer;
			if (!block.predecessors.empty())
			{
				out += " preds=";
				for (size_t i = 0; i < block.predecessors.size(); ++i)
				{
					if (i)
						out += ",";
					std::snprintf(buffer, sizeof(buffer), "b%u", block.predecessors[i]);
					out += buffer;
				}
			}
			out += "\n";

			for (const Inst& inst : block.insts)
			{
				out += "  ";
				if (inst.value != 0)
				{
					AppendValue(out, fn, inst.value);
					out += ": ";
					out += TypeName(inst.type);
					out += " = ";
				}
				out += OpName(inst.op);

				if (inst.arg_count() > 0)
				{
					out += " ";
					for (u32 i = 0; i < inst.arg_count(); ++i)
					{
						if (i)
							out += ", ";
						AppendValue(out, fn, inst.args[i]);
					}
				}

				std::string imm_text;
				AppendImmediate(imm_text, inst);
				if (!imm_text.empty())
				{
					out += " ";
					out += imm_text;
				}

				if (inst.guest_pc)
				{
					std::snprintf(buffer, sizeof(buffer), " @0x%08x", inst.guest_pc);
					out += buffer;
				}
				out += "\n";
			}
		}
	}

	std::string Dump(const Function& fn)
	{
		std::string out;
		Dump(fn, out);
		return out;
	}
} // namespace ir

// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+

#include "ir/Ir.h"

#include <cstring>

namespace ir
{
	const char* TypeName(Type type)
	{
		switch (type)
		{
			case Type::Void: return "void";
			case Type::I32: return "i32";
			case Type::I64: return "i64";
			case Type::F32: return "f32";
			case Type::V4F32: return "v4f32";
			case Type::V4I32: return "v4i32";
			case Type::V4U32: return "v4u32";
			case Type::Flags: return "flags";
			case Type::Any: return "any";
		}
		return "?";
	}

	bool IsVectorType(Type type)
	{
		return type == Type::V4F32 || type == Type::V4I32 || type == Type::V4U32;
	}

	bool IsIntegerType(Type type)
	{
		return type == Type::I32 || type == Type::I64;
	}

	u32 TypeSize(Type type)
	{
		switch (type)
		{
			case Type::Void: return 0;
			case Type::I32: return 4;
			case Type::I64: return 8;
			case Type::F32: return 4;
			case Type::Flags: return 4;
			case Type::V4F32:
			case Type::V4I32:
			case Type::V4U32: return 16;
			case Type::Any: return 0;
		}
		return 0;
	}

	const char* OpName(Op op)
	{
		static constexpr const char* names[] = {
#define IR_OP_NAME(name, kind, min_args, max_args, result, term) #name,
			IR_OP_LIST(IR_OP_NAME)
#undef IR_OP_NAME
		};
		const size_t index = static_cast<size_t>(op);
		return (index < std::size(names)) ? names[index] : "?";
	}

	// ------------------------------------------------------------------
	// Builder
	// ------------------------------------------------------------------
	u32 Builder::AddValue(Type type)
	{
		const u32 id = m_fn->next_value++;
		m_fn->value_types.push_back(type);
		return id;
	}

	void Builder::AddPredecessor(u32 block_id, u32 pred_id)
	{
		Block* block = m_fn->GetBlock(block_id);
		if (!block)
			return;

		for (const u32 existing : block->predecessors)
		{
			if (existing == pred_id)
				return;
		}
		block->predecessors.push_back(pred_id);
	}

	void Builder::AttachSuccessors(Block& block, const Inst& term)
	{
		auto add_successor = [&](u32 target) {
			if (target == 0)
				return;

			for (const u32 existing : block.successors)
			{
				if (existing == target)
					return;
			}
			block.successors.push_back(target);
			AddPredecessor(target, block.id);
		};

		switch (term.op)
		{
			case Op::Jump:
				add_successor(term.args[0]);
				break;
			case Op::Branch:
				add_successor(term.args[1]);
				add_successor(term.args[2]);
				break;
			case Op::CheckEvents:
				add_successor(term.args[0]);
				break;
			default:
				break;
		}
	}

	u32 Builder::CreateBlock(u32 guest_start)
	{
		Block block;
		block.id = static_cast<u32>(m_fn->blocks.size()) + 1;
		block.guest_start = guest_start;
		block.guest_end = guest_start;
		m_fn->blocks.push_back(std::move(block));
		if (m_fn->entry == 0)
			m_fn->entry = static_cast<u32>(m_fn->blocks.size());
		m_block = static_cast<u32>(m_fn->blocks.size());
		m_pc = 0;
		return m_block;
	}

	void Builder::SetBlockEnd(u32 guest_end)
	{
		if (m_block == 0)
			return;
		m_fn->blocks[m_block - 1].guest_end = guest_end;
	}

	u32 Builder::Emit(Op op, Type type, std::initializer_list<u32> args, u64 imm, u16 aux, u32 guest_pc)
	{
		const OpInfo& info = Info(op);
		if (m_block == 0 || args.size() > 4)
		{
			// Malformed construction; the verifier reports it on the caller's
			// behalf, but keep the builder safe.
			return 0;
		}

		Block& block = m_fn->blocks[m_block - 1];
		if (!block.insts.empty() && IsTerminator(block.insts.back().op))
		{
			// Nothing may follow a terminator.
			return 0;
		}

		Inst inst;
		inst.op = op;
		inst.type = type;
		inst.imm = imm;
		inst.aux = aux;
		if (m_delay_slot)
			inst.aux |= IF_DELAY_SLOT;
		inst.guest_pc = guest_pc ? guest_pc : (m_pc ? m_pc : block.guest_start);
		inst.num_args = static_cast<u8>(args.size());
		u32 index = 0;
		for (const u32 arg : args)
			inst.args[index++] = arg;

		if (info.result != Type::Void && !info.terminator)
			inst.value = AddValue(type);

		if (info.terminator)
			AttachSuccessors(block, inst);

		block.insts.push_back(inst);
		if (info.terminator)
			block.guest_end = inst.guest_pc + 4;

		return inst.value;
	}

	u32 Builder::Emit0(Op op, Type type)
	{
		return Emit(op, type, {});
	}

	u32 Builder::Emit1(Op op, Type type, u32 a, u64 imm, u16 aux)
	{
		return Emit(op, type, {a}, imm, aux);
	}

	u32 Builder::Emit2(Op op, Type type, u32 a, u32 b, u64 imm, u16 aux)
	{
		return Emit(op, type, {a, b}, imm, aux);
	}

	u32 Builder::Emit3(Op op, Type type, u32 a, u32 b, u32 c, u64 imm, u16 aux)
	{
		return Emit(op, type, {a, b, c}, imm, aux);
	}

	u32 Builder::Emit4(Op op, Type type, u32 a, u32 b, u32 c, u32 d, u64 imm, u16 aux)
	{
		return Emit(op, type, {a, b, c, d}, imm, aux);
	}

	u32 Builder::ConstI32(u32 v)
	{
		return Emit(Op::ConstI32, Type::I32, {}, v);
	}

	u32 Builder::ConstI64(u64 v)
	{
		return Emit(Op::ConstI64, Type::I64, {}, v);
	}

	u32 Builder::ConstF32(float v)
	{
		u32 bits;
		std::memcpy(&bits, &v, sizeof(bits));
		return Emit(Op::ConstF32, Type::F32, {}, bits);
	}

	u32 Builder::ConstVec(const std::array<u32, 4>& lanes)
	{
		m_fn->vec_consts.push_back(lanes);
		return Emit(Op::ConstVec, Type::V4U32, {}, m_fn->vec_consts.size() - 1);
	}

	u32 Builder::Undef(Type type)
	{
		return Emit(Op::Undef, type, {});
	}

	u32 Builder::Jump(u32 target, u32 guest_target)
	{
		return Emit(Op::Jump, Type::Void, {target}, guest_target);
	}

	u32 Builder::Branch(u32 cond, u32 taken, u32 not_taken, u32 guest_target)
	{
		return Emit(Op::Branch, Type::Void, {cond, taken, not_taken}, guest_target);
	}

	u32 Builder::BranchIndirect(u32 address)
	{
		return Emit(Op::BranchIndirect, Type::Void, {address});
	}

	u32 Builder::CheckEvents(u32 fallthrough_block)
	{
		return Emit(Op::CheckEvents, Type::Void, {fallthrough_block});
	}

	u32 Builder::Resume(u32 guest_pc)
	{
		return Emit(Op::Resume, Type::Void, {}, guest_pc);
	}

	u32 Builder::Return()
	{
		return Emit(Op::Return, Type::Void, {});
	}

	u32 Builder::Trap()
	{
		return Emit(Op::Trap, Type::Void, {});
	}
} // namespace ir

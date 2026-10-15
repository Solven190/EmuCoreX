// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+
// Strict common-IR VU FTOI planner/lowerer oracle. Include after the
// BoundVfToInt APIs and native counter seam are declared.
#include "arm64/ir/IrLower-arm64.h"

#include <array>
#include <limits>
#include <vector>

extern "C" u64 EmuCoreXOracleVUIRFtoiInstructions(u32 index);

static ir::Op VuFtoiOp(u32 fractional_bits)
{
	switch (fractional_bits)
	{
		case 0: return ir::Op::VF2I0;
		case 4: return ir::Op::VF2I4;
		case 12: return ir::Op::VF2I12;
		case 15: return ir::Op::VF2I15;
		default: return ir::Op::Nop;
	}
}

static u32 VuFtoiExpectedBits(u32 raw_bits, u32 fractional_bits, u32 fpcr_bits)
{
	// Independent integer model of trunc(x * 2^fractional_bits), saturated to
	// signed 32-bit range. fpcr_bits is accepted for callers' configuration
	// bookkeeping; FZ/DAZ subnormals always truncate to zero. A scaled NaN with
	// DN enabled becomes the positive default NaN before legacy saturation; FTOI0
	// has no scaling multiply and therefore retains the raw NaN sign.
	const bool default_nan = (fpcr_bits & (1u << 25)) != 0;
	const bool negative = (raw_bits >> 31) != 0;
	const u32 exponent = (raw_bits >> 23) & 0xffu;
	const u32 fraction = raw_bits & 0x007fffffu;
	if (fractional_bits != 0 && fractional_bits != 4 &&
		fractional_bits != 12 && fractional_bits != 15)
		return 0;
	if (exponent == 0xffu)
	{
		if (fraction != 0 && fractional_bits != 0 && default_nan)
			return 0x7fffffffu;
		return negative ? 0x80000000u : 0x7fffffffu;
	}
	if (exponent == 0)
		return 0;
	if (exponent + fractional_bits >= 158u)
		return negative ? 0x80000000u : 0x7fffffffu;

	const u32 significand = 0x00800000u | fraction;
	const s32 shift = static_cast<s32>(exponent) + static_cast<s32>(fractional_bits) - 150;
	u32 magnitude = 0;
	if (shift >= 0)
		magnitude = significand << static_cast<u32>(shift); // at most seven bits here
	else
	{
		const u32 right_shift = static_cast<u32>(-shift);
		magnitude = right_shift >= 32 ? 0 : significand >> right_shift;
	}
	return negative ? 0u - magnitude : magnitude;
}

static std::vector<u32> VuFtoiRawCorpus()
{
	std::vector<u32> inputs = {
		0x00000000u, 0x80000000u, 0x00000001u, 0x007fffffu,
		0x80000001u, 0x807fffffu, 0x3f800000u, 0xbf800000u,
		0x3f000000u, 0xbf000000u, 0x3fc00000u, 0xbfc00000u,
		0x4effffffu, 0xceffffffu, 0x4f000000u, 0xcf000000u,
		0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u,
		0x7fc12345u, 0xffc54321u, 0x7f812345u, 0xff812345u};
	for (const u32 fractional_bits : {0u, 4u, 12u, 15u})
	{
		// Raw IEEE encodings for +/-2^(31-fractional_bits), and the adjacent
		// representable values on either side of each signed saturation edge.
		const u32 threshold = 0x4f000000u - (fractional_bits << 23);
		for (const u32 delta : {u32(-1), 0u, 1u})
		{
			const u32 positive = threshold + delta;
			inputs.push_back(positive);
			inputs.push_back(positive | 0x80000000u);
		}
	}
	return inputs;
}

static ir::Function BoundVfToIntFixture(u32 src, u32 dst, u32 mask,
	u32 fractional_bits, bool delay_slot = false)
{
	ir::Function fn;
	ir::Builder b(fn);
	b.CreateBlock(0x80);
	b.SetDelaySlot(delay_slot);
	const u32 source = b.Emit(ir::Op::ReadVf, ir::Type::V4F32, {}, src);
	const u32 converted = b.Emit1(VuFtoiOp(fractional_bits), ir::Type::V4I32, source);
	b.Emit1(ir::Op::WriteVfBitsMasked, ir::Type::Void, converted, dst,
		static_cast<u16>(mask | (delay_slot ? ir::IF_DELAY_SLOT : 0)));
	b.Return();
	return fn;
}

static bool VuFtoiDirectMatrix(const std::array<Arm64Ir::BoundVfToIntPlan, 4 * 15>& plans)
{
	constexpr std::array<u32, 4> fractions = {0, 4, 12, 15};
	constexpr std::array<FPRoundMode, 4> round_modes = {
		FPRoundMode::Nearest, FPRoundMode::PositiveInfinity,
		FPRoundMode::NegativeInfinity, FPRoundMode::ChopZero};
	const std::vector<u32> raw_inputs = VuFtoiRawCorpus();
	const std::array<u32, 4> initial_dst = {0x13579bdfu, 0x2468ace0u, 0xdeadbeefu, 0x01234567u};
	bool correct = true;

	for (u32 fi = 0; fi < fractions.size(); ++fi)
	for (u32 mask = 1; mask < 16; ++mask)
	for (u32 alias = 0; alias < 2; ++alias)
	for (Arm64Ir::BoundVfConvertLayout layout : {Arm64Ir::BoundVfConvertLayout::GuestLanes,
		Arm64Ir::BoundVfConvertLayout::MicroVuPartial})
	{
		std::array<u32, 4> src{};
		std::array<u32, 4> actual{};
		oakSetAsmPtr(SysMemory::GetEERec() + 0x2000000, 0x10000);
		u8* entry = oakStartBlock();
		if (alias)
			oakAsm->LDR(oak::QReg(4), oak::util::X0);
		else
		{
			oakAsm->LDR(oak::QReg(3), oak::util::X0);
			oakAsm->LDR(oak::QReg(4), oak::util::X1);
		}
		const u8* before = oakGetCurrentCodePointer();
		Arm64Ir::LowerOutput out;
		std::string error;
		const bool lowered = Arm64Ir::LowerBoundVfToIntPlan(plans[fi * 15 + mask - 1],
			alias ? 4u : 3u, 4u, &out, &error, layout);
		const size_t emitted = static_cast<size_t>(oakGetCurrentCodePointer() - before);
		if (lowered)
			oakAsm->STR(oak::QReg(4), oak::util::X2);
		oakAsm->RET();
		oakEndBlock();
		correct &= lowered && emitted != 0 && out.host_size == emitted &&
			out.frame_size == 0 && out.spill_values == 0;
		if (!lowered)
			continue;

		for (u32 mode = 0; mode < round_modes.size(); ++mode)
		for (u32 fz = 0; fz < 2; ++fz)
		for (u32 dn = 0; dn < 2; ++dn)
		{
			FPControlRegister fpcr = EmuConfig.Cpu.FPUFPCR;
			fpcr.SetRoundMode(round_modes[mode]);
			fpcr.SetFlushToZero(fz != 0);
			if (dn)
				fpcr.bitmask |= (1ull << 25);
			else
				fpcr.bitmask &= ~(1ull << 25);
			const FPControlRegisterBackup restore_fpcr(fpcr);
			for (u32 base = 0; base < raw_inputs.size(); base += 4)
			{
				for (u32 lane = 0; lane < 4; ++lane)
					src[lane] = raw_inputs[(base + lane) % raw_inputs.size()];
				actual.fill(0);
				reinterpret_cast<void (*)(const u32*, const u32*, u32*)>(entry)(
					src.data(), alias ? src.data() : initial_dst.data(), actual.data());
				const bool one_lane = (mask & (mask - 1)) == 0;
				for (u32 lane = 0; lane < 4; ++lane)
				{
					if (layout == Arm64Ir::BoundVfConvertLayout::GuestLanes)
					{
						const u32 expected = (mask & (8u >> lane)) ?
							VuFtoiExpectedBits(src[lane], fractions[fi], fpcr.bitmask) :
							(alias ? src[lane] : initial_dst[lane]);
						correct &= actual[lane] == expected;
					}
					else if (mask & (8u >> lane))
					{
						const u32 output_lane = one_lane ? 0u : lane;
						correct &= actual[output_lane] == VuFtoiExpectedBits(src[lane],
							fractions[fi], fpcr.bitmask);
					}
				}
			}
		}
	}
	return correct;
}

static void VUBoundVfToIntTests()
{
	constexpr std::array<u32, 4> fractions = {0, 4, 12, 15};
	const std::vector<u32> model_inputs = VuFtoiRawCorpus();
	bool model_ok = VuFtoiExpectedBits(0x3f800000u, 0, 0) == 1u &&
		VuFtoiExpectedBits(0xbf800000u, 0, 0) == 0xffffffffu &&
		VuFtoiExpectedBits(0x3f800000u, 4, 0) == 16u &&
		VuFtoiExpectedBits(0xbf800000u, 15, 0) == 0xffff8000u &&
		VuFtoiExpectedBits(0x4f000000u, 0, 0) == 0x7fffffffu &&
		VuFtoiExpectedBits(0xcf000000u, 0, 0) == 0x80000000u &&
		VuFtoiExpectedBits(0x4effffffu, 0, 0) == 0x7fffff80u &&
		VuFtoiExpectedBits(0xceffffffu, 0, 0) == 0x80000080u &&
		VuFtoiExpectedBits(0xff812345u, 0, 1u << 25) == 0x80000000u &&
		VuFtoiExpectedBits(0xff812345u, 4, 0) == 0x80000000u &&
		VuFtoiExpectedBits(0xff812345u, 4, 1u << 25) == 0x7fffffffu &&
		VuFtoiExpectedBits(0xff800000u, 4, 1u << 25) == 0x80000000u;
	for (u32 offset : fractions)
	for (u32 bits : model_inputs)
		(void)VuFtoiExpectedBits(bits, offset, 0); // Exercise normal, tiny, saturation, Inf and NaN paths.
	Check(model_ok, "VU FTOI expected model covers exact scale and signed saturation boundaries");

	std::array<Arm64Ir::BoundVfToIntPlan, 4 * 15> plans{};
	bool planner_ok = true;
	for (u32 fi = 0; fi < fractions.size(); ++fi)
	for (u32 mask = 1; mask < 16; ++mask)
	{
		std::string error;
		Arm64Ir::BoundVfToIntPlan plan;
		const ir::Function fn = BoundVfToIntFixture(7, 9, mask, fractions[fi]);
		const bool accepted = ir::Verify(fn, &error) && Arm64Ir::PlanBoundVfToInt(fn, &plan, &error);
		planner_ok &= accepted && plan.src_vf == 7 && plan.dst_vf == 9 &&
			plan.lane_mask == mask && plan.fractional_bits == fractions[fi] && !plan.delay_slot;
		if (accepted)
			plans[fi * 15 + mask - 1] = plan;
	}
	Check(planner_ok, "shared VU FTOI planner admits all offsets and 15 lane masks");
	if (planner_ok)
	{
		bool generic_rejects = true;
		for (const Arm64Ir::GuestState state : {Arm64Ir::GuestState::EE, Arm64Ir::GuestState::IOP})
		{
			auto fn = BoundVfToIntFixture(7, 9, 15, 4);
			std::string generic_error;
			generic_rejects &= !Arm64Ir::CanLower(fn, false, &generic_error, state);
			Arm64Ir::LowerOptions options;
			options.guest_state = state;
			std::array<u8, 512> code;
			code.fill(0xa5);
			Arm64Ir::LowerOutput out{reinterpret_cast<u8*>(1), 1, 1};
			const bool lowered = Arm64Ir::LowerBlock(fn, options, code.data(), code.size(), &out,
				&generic_error);
			generic_rejects &= !lowered && out.entry == nullptr && out.host_size == 0 &&
				out.frame_size == 0 && out.spill_values == 0 &&
				std::all_of(code.begin(), code.end(), [](u8 byte) { return byte == 0xa5; });
		}
	Check(generic_rejects,
		"generic EE and IOP backends reject valid VU FTOI without emitting code");
	}
	Check(planner_ok && VuFtoiDirectMatrix(plans),
		"shared VU FTOI raw lowerer matches integer model for alias/layout/mask/FPCR matrix");
	Check(EmuCoreXOracleVUIRFtoiInstructions(0) == 0 &&
		EmuCoreXOracleVUIRFtoiInstructions(1) == 0,
		"shared VU FTOI production counters remain zero before upper routing");

	std::string error;
	Arm64Ir::BoundVfToIntPlan valid_plan;
	const bool valid = Arm64Ir::PlanBoundVfToInt(
		BoundVfToIntFixture(7, 9, 15, 4), &valid_plan, &error);
	bool rejects = valid;
	const auto reject = [&](ir::Function fn) {
		Arm64Ir::BoundVfToIntPlan plan{1, 2, 3, 4, true};
		const bool accepted = Arm64Ir::PlanBoundVfToInt(fn, &plan, &error);
		rejects &= !accepted && plan.src_vf == 0 && plan.dst_vf == 0 &&
			plan.lane_mask == 0 && plan.fractional_bits == 0 && !plan.delay_slot;
	};
	for (u32 mask : {0u, 16u}) reject(BoundVfToIntFixture(7, 9, mask, 4));
	reject(BoundVfToIntFixture(32, 9, 15, 4));
	reject(BoundVfToIntFixture(7, 32, 15, 4));
	reject(BoundVfToIntFixture(7, 0, 15, 4));
	reject(BoundVfToIntFixture(7, 9, 15, 1));
	{
		auto fn = BoundVfToIntFixture(0, 9, 15, 4);
		Arm64Ir::BoundVfToIntPlan plan;
		rejects &= ir::Verify(fn, &error) && Arm64Ir::PlanBoundVfToInt(fn, &plan, &error) &&
			plan.src_vf == 0 && plan.dst_vf == 9;
	}
	if (valid)
	{
		auto fn = BoundVfToIntFixture(7, 9, 15, 4);
		fn.blocks[0].insts[0].op = ir::Op::ReadVfBits;
		reject(std::move(fn));
		fn = BoundVfToIntFixture(7, 9, 15, 4);
		fn.blocks[0].insts[1].op = ir::Op::VI2F4;
		reject(std::move(fn));
		fn = BoundVfToIntFixture(7, 9, 15, 4);
		fn.blocks[0].insts[1].imm = 1;
		reject(std::move(fn));
		fn = BoundVfToIntFixture(7, 9, 15, 4);
		fn.blocks[0].insts[0].aux = ir::IF_EXACT;
		reject(std::move(fn));
		fn = BoundVfToIntFixture(7, 9, 15, 4);
		fn.blocks[0].insts[2].args[0] = fn.blocks[0].insts[0].value;
		reject(std::move(fn));
		fn = BoundVfToIntFixture(7, 9, 15, 4);
		fn.blocks[0].insts[2].aux |= 0x10;
		reject(std::move(fn));
		fn = BoundVfToIntFixture(7, 9, 15, 4);
		ir::Inst extra;
		extra.op = ir::Op::Nop;
		fn.blocks[0].insts.insert(fn.blocks[0].insts.end() - 1, extra);
		reject(std::move(fn));
		fn = BoundVfToIntFixture(7, 9, 15, 4);
		ir::Builder extra_block(fn);
		extra_block.CreateBlock(0x90);
		extra_block.Return();
		reject(std::move(fn));
		fn = BoundVfToIntFixture(7, 9, 15, 4, true);
		fn.blocks[0].insts[2].aux &= ~ir::IF_DELAY_SLOT;
		reject(std::move(fn));
	}
	Check(rejects, "shared VU FTOI planner clears and rejects malformed plans, fields, and shapes");
	{
		std::string delay_error;
		Arm64Ir::BoundVfToIntPlan delay_plan;
		const ir::Function delay_fn = BoundVfToIntFixture(7, 9, 10, 12, true);
		Check(ir::Verify(delay_fn, &delay_error) &&
			Arm64Ir::PlanBoundVfToInt(delay_fn, &delay_plan, &delay_error) &&
			delay_plan.delay_slot && (delay_fn.blocks[0].insts[0].aux & ir::IF_DELAY_SLOT) &&
			(delay_fn.blocks[0].insts[1].aux & ir::IF_DELAY_SLOT) &&
			(delay_fn.blocks[0].insts[2].aux & ir::IF_DELAY_SLOT),
			"shared VU FTOI plan carries consistent delay-slot metadata");
	}

	if (valid)
	{
		for (const std::array<u32, 2> binding : {std::array<u32, 2>{31, 4}, {3, 31},
			{32, 4}, {static_cast<u32>(OAK_QSCRATCH.index()), 4},
			{3, static_cast<u32>(OAK_QSCRATCH.index())},
			{static_cast<u32>(OAK_QSCRATCH2.index()), 4},
			{3, static_cast<u32>(OAK_QSCRATCH2.index())},
			{static_cast<u32>(OAK_QSCRATCH3.index()), 4},
			{3, static_cast<u32>(OAK_QSCRATCH3.index())}})
		{
			oakSetAsmPtr(SysMemory::GetEERec() + 0x2000000, 0x10000);
			oakStartBlock();
			const u8* before = oakGetCurrentCodePointer();
			Arm64Ir::LowerOutput out{reinterpret_cast<u8*>(1), 1, 1};
			const bool lowered = Arm64Ir::LowerBoundVfToIntPlan(valid_plan, binding[0], binding[1],
				&out, &error, Arm64Ir::BoundVfConvertLayout::MicroVuPartial);
			const bool no_code = before == oakGetCurrentCodePointer() && out.host_size == 0 &&
				out.frame_size == 0 && out.entry == nullptr;
			oakAsm->RET();
			oakEndBlock();
			Check(!lowered && no_code, "shared VU FTOI rejects invalid Q-register binding without emission");
		}
		for (const Arm64Ir::BoundVfToIntPlan bad_plan : {
			Arm64Ir::BoundVfToIntPlan{32, 9, 15, 4, false},
			Arm64Ir::BoundVfToIntPlan{7, 0, 15, 4, false},
			Arm64Ir::BoundVfToIntPlan{7, 9, 0, 4, false},
			Arm64Ir::BoundVfToIntPlan{7, 9, 16, 4, false},
			Arm64Ir::BoundVfToIntPlan{7, 9, 15, 8, false}})
		{
			oakSetAsmPtr(SysMemory::GetEERec() + 0x2000000, 0x10000);
			oakStartBlock();
			const u8* before = oakGetCurrentCodePointer();
			Arm64Ir::LowerOutput out{reinterpret_cast<u8*>(1), 1, 1};
			const bool lowered = Arm64Ir::LowerBoundVfToIntPlan(bad_plan, 3, 4,
				&out, &error, Arm64Ir::BoundVfConvertLayout::MicroVuPartial);
			const bool no_code = before == oakGetCurrentCodePointer() && out.host_size == 0 &&
				out.frame_size == 0 && out.entry == nullptr;
			oakAsm->RET();
			oakEndBlock();
			Check(!lowered && no_code, "shared VU FTOI rejects malformed manual plan without emission");
		}
		oakSetAsmPtr(SysMemory::GetEERec() + 0x2000000, 0x10000);
		oakStartBlock();
		const u8* before = oakGetCurrentCodePointer();
		Arm64Ir::LowerOutput out{reinterpret_cast<u8*>(1), 1, 1};
		const bool lowered = Arm64Ir::LowerBoundVfToIntPlan(valid_plan, 3, 4, &out, &error,
			static_cast<Arm64Ir::BoundVfConvertLayout>(2));
		const bool no_code = before == oakGetCurrentCodePointer() && out.host_size == 0 &&
			out.frame_size == 0 && out.entry == nullptr;
		oakAsm->RET();
		oakEndBlock();
		Check(!lowered && no_code, "shared VU FTOI rejects unknown layout without emission");
	}
}

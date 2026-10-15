// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+
//
// Test-first common-IR ITOF oracle fixtures. Production routing and the real
// bound ARM64 conversion emitter are deliberately not present in RED phase.
#include "arm64/ir/IrLower-arm64.h"

#include <array>
#include <bit>
#include <limits>

extern "C" u64 EmuCoreXOracleVUIRItofInstructions(u32 index);
void VUSharedItofIntegrationTests();
void EEVuItofMacroTests();

static constexpr std::array<u32, 13> kVuItofRawInputs = {
	0x00000000u, 0x00000001u, 0xffffffffu, 0x80000000u, 0x7fffffffu,
	0x00ffffffu, 0xff000001u, 0x01000000u, 0xff000000u,
	0x01000001u, 0xfeffffffu, 0x01000003u, 0xfefffffdu};

static ir::Op VuItofOp(u32 fractional_bits)
{
	switch (fractional_bits)
	{
		case 0: return ir::Op::VI2F0;
		case 4: return ir::Op::VI2F4;
		case 12: return ir::Op::VI2F12;
		case 15: return ir::Op::VI2F15;
		default: return ir::Op::Nop;
	}
}

static u32 VuItofFraction(ir::Op op)
{
	switch (op)
	{
		case ir::Op::VI2F0: return 0;
		case ir::Op::VI2F4: return 4;
		case ir::Op::VI2F12: return 12;
		case ir::Op::VI2F15: return 15;
		default: return 0xffffffffu;
	}
}

enum class VuItofRounding : u8
{
	NearestEven,
	PlusInfinity,
	MinusInfinity,
	TowardZero,
};

static u32 VuItofExpectedBits(u32 raw_signed_bits, u32 fractional_bits, VuItofRounding mode)
{
	const bool negative = (raw_signed_bits >> 31) != 0;
	const u32 magnitude = negative ? (0u - raw_signed_bits) : raw_signed_bits;
	if (magnitude == 0)
		return 0;

	u32 exponent = 31u - std::countl_zero(magnitude);
	u32 significand;
	if (exponent <= 23)
	{
		significand = magnitude << (23u - exponent);
	}
	else
	{
		const u32 shift = exponent - 23u;
		significand = magnitude >> shift;
		const u32 remainder = magnitude & ((1u << shift) - 1u);
		const u32 halfway = 1u << (shift - 1u);
		bool increment = false;
		switch (mode)
		{
			case VuItofRounding::NearestEven:
				increment = remainder > halfway || (remainder == halfway && (significand & 1u));
				break;
			case VuItofRounding::PlusInfinity:
				increment = !negative && remainder != 0;
				break;
			case VuItofRounding::MinusInfinity:
				increment = negative && remainder != 0;
				break;
			case VuItofRounding::TowardZero:
				break;
		}
		if (increment && ++significand == 0x01000000u)
		{
			significand >>= 1;
			++exponent;
		}
	}

	const u32 sign = negative ? 0x80000000u : 0;
	const u32 exponent_field = (exponent - fractional_bits + 127u) << 23;
	return sign | exponent_field | (significand & 0x007fffffu);
}

static FPRoundMode VuItofFpcrMode(VuItofRounding mode)
{
	switch (mode)
	{
		case VuItofRounding::NearestEven: return FPRoundMode::Nearest;
		case VuItofRounding::PlusInfinity: return FPRoundMode::PositiveInfinity;
		case VuItofRounding::MinusInfinity: return FPRoundMode::NegativeInfinity;
		case VuItofRounding::TowardZero: return FPRoundMode::ChopZero;
	}
	return FPRoundMode::Nearest;
}

static ir::Function BoundVfConvertFixture(u32 src, u32 dst, u32 mask,
	u32 fractional_bits, bool delay_slot = false)
{
	ir::Function fn;
	ir::Builder b(fn);
	b.CreateBlock(0x80);
	b.SetDelaySlot(delay_slot);
	const u32 source = b.Emit(ir::Op::ReadVfBits, ir::Type::V4I32, {}, src);
	const u32 converted = b.Emit1(VuItofOp(fractional_bits), ir::Type::V4F32, source);
	b.Emit1(ir::Op::WriteVfMasked, ir::Type::Void, converted, dst,
		static_cast<u16>(mask | (delay_slot ? ir::IF_DELAY_SLOT : 0)));
	b.Return();
	return fn;
}

static void VUBoundVfConvertTests()
{
	constexpr std::array<u32, 4> fractions = {0, 4, 12, 15};
	constexpr std::array<VuItofRounding, 4> modes = {
		VuItofRounding::NearestEven, VuItofRounding::PlusInfinity,
		VuItofRounding::MinusInfinity, VuItofRounding::TowardZero};
	constexpr std::array<FPRoundMode, 4> fpcr_modes = {
		FPRoundMode::Nearest, FPRoundMode::PositiveInfinity,
		FPRoundMode::NegativeInfinity, FPRoundMode::ChopZero};

	bool model_ok = VuItofExpectedBits(0, 0, VuItofRounding::NearestEven) == 0;
	constexpr std::array<u32, 4> plus_one = {0x3f800000u, 0x3d800000u, 0x39800000u, 0x38000000u};
	for (u32 i = 0; i < fractions.size(); ++i)
	{
		model_ok &= VuItofExpectedBits(1, fractions[i], VuItofRounding::NearestEven) == plus_one[i];
		model_ok &= VuItofExpectedBits(0xffffffffu, fractions[i], VuItofRounding::NearestEven) ==
			(plus_one[i] | 0x80000000u);
	}
	for (u32 fraction : fractions)
	for (u32 input : kVuItofRawInputs)
	for (VuItofRounding mode : modes)
		(void)VuItofExpectedBits(input, fraction, mode); // Exercise every model branch without host FP.
	model_ok &= VuItofExpectedBits(0x01000001u, 0, VuItofRounding::NearestEven) == 0x4b800000u;
	model_ok &= VuItofExpectedBits(0x01000003u, 0, VuItofRounding::NearestEven) == 0x4b800002u;
	model_ok &= VuItofExpectedBits(0xfeffffffu, 0, VuItofRounding::NearestEven) == 0xcb800000u;
	model_ok &= VuItofExpectedBits(0xfefffffdu, 0, VuItofRounding::NearestEven) == 0xcb800002u;
	model_ok &= VuItofExpectedBits(0x01000001u, 0, VuItofRounding::PlusInfinity) == 0x4b800001u;
	model_ok &= VuItofExpectedBits(0x01000001u, 0, VuItofRounding::MinusInfinity) == 0x4b800000u;
	model_ok &= VuItofExpectedBits(0xfeffffffu, 0, VuItofRounding::PlusInfinity) == 0xcb800000u;
	model_ok &= VuItofExpectedBits(0xfeffffffu, 0, VuItofRounding::MinusInfinity) == 0xcb800001u;
	Check(model_ok, "VU ITOF integer-only expected-bit model exact cases and rounding branches");

	std::array<Arm64Ir::BoundVfConvertPlan, 4 * 15> plans{};
	bool all_plans = true;
	for (u32 v = 0; v < fractions.size(); ++v)
	for (u32 mask = 1; mask < 16; ++mask)
	{
		std::string error;
		Arm64Ir::BoundVfConvertPlan plan;
		const ir::Function fn = BoundVfConvertFixture(7, 9, mask, fractions[v]);
		const bool accepted = ir::Verify(fn, &error) && Arm64Ir::PlanBoundVfConvert(fn, &plan, &error);
		all_plans &= accepted && plan.src_vf == 7 && plan.dst_vf == 9 && plan.lane_mask == mask &&
			plan.fractional_bits == fractions[v] && !plan.delay_slot;
		if (accepted)
			plans[v * 15 + mask - 1] = plan;
	}
	Check(all_plans, "shared VU ITOF planner accepts four offsets and all 15 lane masks");

	bool direct_lowered_and_correct = all_plans;
	if (all_plans)
	{
		const std::array<u32, 4> initial = {0x13579bdfu, 0x2468ace0u, 0xdeadbeefu, 0x01234567u};
		for (u32 v = 0; v < fractions.size(); ++v)
		for (u32 mask = 1; mask < 16; ++mask)
		for (u32 alias = 0; alias < 2; ++alias)
		for (Arm64Ir::BoundVfConvertLayout layout : {Arm64Ir::BoundVfConvertLayout::GuestLanes,
			Arm64Ir::BoundVfConvertLayout::MicroVuPartial})
		{
			std::array<u32, 4> src{};
			for (u32 lane = 0; lane < 4; ++lane)
				src[lane] = kVuItofRawInputs[(mask + lane * 3 + v) % kVuItofRawInputs.size()];
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
			const bool lowered = Arm64Ir::LowerBoundVfConvertPlan(plans[v * 15 + mask - 1],
				alias ? 4 : 3, 4, &out, &error, layout);
			const size_t emitted = static_cast<size_t>(oakGetCurrentCodePointer() - before);
			if (lowered)
				oakAsm->STR(oak::QReg(4), oak::util::X2);
			oakAsm->RET();
			oakEndBlock();
			direct_lowered_and_correct &= lowered && emitted != 0 && out.host_size == emitted &&
				out.frame_size == 0 && out.spill_values == 0;
			if (!lowered)
				continue;

			for (u32 mode_index = 0; mode_index < modes.size(); ++mode_index)
		{
			FPControlRegister fpcr = EmuConfig.Cpu.FPUFPCR;
			fpcr.SetRoundMode(fpcr_modes[mode_index]);
			const FPControlRegisterBackup restoreFpcr(fpcr);
			for (u32 base = 0; base < kVuItofRawInputs.size(); base += 4)
			{
				for (u32 lane = 0; lane < 4; ++lane)
					src[lane] = kVuItofRawInputs[(base + lane) % kVuItofRawInputs.size()];
				actual.fill(0);
				reinterpret_cast<void (*)(const u32*, const u32*, u32*)>(entry)(
					src.data(), alias ? src.data() : initial.data(), actual.data());
				const bool one_lane = (mask & (mask - 1)) == 0;
				for (u32 lane = 0; lane < 4; ++lane)
				{
					if (layout == Arm64Ir::BoundVfConvertLayout::GuestLanes)
					{
						const u32 expected = (mask & (8u >> lane)) ?
							VuItofExpectedBits(src[lane], fractions[v], modes[mode_index]) :
							(alias ? src[lane] : initial[lane]);
						direct_lowered_and_correct &= actual[lane] == expected;
					}
					else if (mask & (8u >> lane))
					{
						const u32 output_lane = one_lane ? 0 : lane;
						direct_lowered_and_correct &= actual[output_lane] ==
							VuItofExpectedBits(src[lane], fractions[v], modes[mode_index]);
					}
				}
			}
		}
		}
		Check(direct_lowered_and_correct,
			"shared VU ITOF lowers all offsets, masks, aliases, partial layouts, and FPCR modes");
	}

	bool counters_zero = EmuCoreXOracleVUIRItofInstructions(0) == 0 &&
		EmuCoreXOracleVUIRItofInstructions(1) == 0;
	Check(counters_zero, "shared VU ITOF production counters remain zero before upper routing");

	// The planner must reject malformed fixed slices and clear partial outputs.
	auto valid_fn = BoundVfConvertFixture(7, 9, 15, 4);
	Arm64Ir::BoundVfConvertPlan valid_plan;
	std::string error;
	const bool valid_plan_ok = Arm64Ir::PlanBoundVfConvert(valid_fn, &valid_plan, &error);
	bool malformed_rejected = valid_plan_ok;
	const auto reject = [&](ir::Function fn) {
		Arm64Ir::BoundVfConvertPlan plan{1, 2, 3, 4, true};
		const bool accepted = Arm64Ir::PlanBoundVfConvert(fn, &plan, &error);
		malformed_rejected &= !accepted && plan.src_vf == 0 && plan.dst_vf == 0 &&
			plan.lane_mask == 0 && plan.fractional_bits == 0 && !plan.delay_slot;
	};
	for (u32 bad_mask : {0u, 16u}) reject(BoundVfConvertFixture(7, 9, bad_mask, 4));
	reject(BoundVfConvertFixture(7, 0, 15, 4));
	reject(BoundVfConvertFixture(32, 9, 15, 4));
	reject(BoundVfConvertFixture(7, 9, 15, 2));
	// VF0 is legal as the raw source (its architectural w lane is 1.0 bits).
	{
		auto fn = BoundVfConvertFixture(0, 9, 15, 4);
		Arm64Ir::BoundVfConvertPlan plan;
		malformed_rejected &= ir::Verify(fn, &error) && Arm64Ir::PlanBoundVfConvert(fn, &plan, &error) &&
			plan.src_vf == 0 && plan.dst_vf == 9;
	}
	{
		auto fn = valid_fn;
		fn.blocks[0].insts[0].op = ir::Op::ReadVf;
		reject(std::move(fn));
	}
	{
		auto fn = valid_fn;
		fn.blocks[0].insts[1].op = ir::Op::VF2I4;
		reject(std::move(fn));
	}
	{
		auto fn = valid_fn;
		fn.blocks[0].insts[0].aux = ir::IF_DELAY_SLOT;
		reject(std::move(fn));
	}
	{
		auto fn = valid_fn;
		fn.blocks[0].insts[1].aux = ir::IF_EXACT;
		reject(std::move(fn));
	}
	{
		auto fn = valid_fn;
		fn.blocks[0].insts[1].imm = 1;
		reject(std::move(fn));
	}
	{
		auto fn = valid_fn;
		// A well-typed ReadVf is a float vector and must not satisfy VI2F input.
		auto& read = fn.blocks[0].insts[0];
		read.op = ir::Op::ReadVf;
		read.type = ir::Type::V4F32;
		fn.value_types[read.value] = ir::Type::V4F32;
		reject(std::move(fn));
	}
	{
		auto fn = valid_fn;
		auto& convert = fn.blocks[0].insts[1];
		convert.type = ir::Type::V4I32;
		fn.value_types[convert.value] = ir::Type::V4I32;
		reject(std::move(fn));
	}
	{
		auto fn = valid_fn;
		fn.blocks[0].insts[0].aux = ir::IF_EXACT;
		reject(std::move(fn));
	}
	{
		auto fn = valid_fn;
		fn.blocks[0].insts[2].args[0] = fn.blocks[0].insts[0].value;
		reject(std::move(fn));
	}
	{
		auto fn = valid_fn;
		fn.blocks[0].insts[2].aux |= 0x10;
		reject(std::move(fn));
	}
	{
		auto fn = valid_fn;
		ir::Inst extra;
		extra.op = ir::Op::Nop;
		fn.blocks[0].insts.insert(fn.blocks[0].insts.end() - 1, extra);
		reject(std::move(fn));
	}
	{
		auto fn = valid_fn;
		ir::Builder b(fn);
		b.CreateBlock(0x90);
		b.Return();
		reject(std::move(fn));
	}
	Check(malformed_rejected, "shared VU ITOF planner rejects malformed types, ops, flags, links, masks, and blocks");

	{
		auto fn = BoundVfConvertFixture(7, 9, 15, 4, true);
		Arm64Ir::BoundVfConvertPlan plan;
		const bool delay_metadata = ir::Verify(fn, &error) && Arm64Ir::PlanBoundVfConvert(fn, &plan, &error) &&
			plan.delay_slot && (fn.blocks[0].insts[0].aux & ir::IF_DELAY_SLOT) &&
			(fn.blocks[0].insts[1].aux & ir::IF_DELAY_SLOT) &&
			(fn.blocks[0].insts[2].aux & ir::IF_DELAY_SLOT);
		Check(delay_metadata, "shared VU ITOF plan preserves consistent delay-slot metadata");
	}

	// Invalid host bindings, plans, and layouts must reject without emitting code.
	if (valid_plan_ok)
	{
		for (const std::array<u32, 2> binding : {std::array<u32, 2>{31, 4}, {3, 31},
			{32, 4}, {static_cast<u32>(OAK_QSCRATCH.index()), 4},
			{3, static_cast<u32>(OAK_QSCRATCH.index())}})
		{
			oakSetAsmPtr(SysMemory::GetEERec() + 0x2000000, 0x10000);
			oakStartBlock();
			const u8* before = oakGetCurrentCodePointer();
			Arm64Ir::LowerOutput out{reinterpret_cast<u8*>(1), 1, 1};
			const bool lowered = Arm64Ir::LowerBoundVfConvertPlan(valid_plan, binding[0], binding[1],
				&out, &error, Arm64Ir::BoundVfConvertLayout::MicroVuPartial);
			const bool no_code = before == oakGetCurrentCodePointer() && out.host_size == 0 &&
				out.frame_size == 0 && out.entry == nullptr;
			oakAsm->RET();
			oakEndBlock();
			Check(!lowered && no_code, "shared VU ITOF rejects invalid host binding without emission");
		}
		for (const Arm64Ir::BoundVfConvertPlan bad_plan : {
			Arm64Ir::BoundVfConvertPlan{32, 9, 15, 4, false},
			Arm64Ir::BoundVfConvertPlan{7, 0, 15, 4, false},
			Arm64Ir::BoundVfConvertPlan{7, 9, 0, 4, false},
			Arm64Ir::BoundVfConvertPlan{7, 9, 16, 4, false},
			Arm64Ir::BoundVfConvertPlan{7, 9, 15, 16, false}})
		{
			oakSetAsmPtr(SysMemory::GetEERec() + 0x2000000, 0x10000);
			oakStartBlock();
			const u8* before = oakGetCurrentCodePointer();
			Arm64Ir::LowerOutput out{reinterpret_cast<u8*>(1), 1, 1};
			const bool lowered = Arm64Ir::LowerBoundVfConvertPlan(bad_plan, 3, 4, &out, &error,
				Arm64Ir::BoundVfConvertLayout::MicroVuPartial);
			const bool no_code = before == oakGetCurrentCodePointer() && out.host_size == 0 &&
				out.frame_size == 0 && out.entry == nullptr;
			oakAsm->RET();
			oakEndBlock();
			Check(!lowered && no_code, "shared VU ITOF rejects malformed manual plan without emission");
		}
		oakSetAsmPtr(SysMemory::GetEERec() + 0x2000000, 0x10000);
		oakStartBlock();
		const u8* before = oakGetCurrentCodePointer();
		Arm64Ir::LowerOutput out{reinterpret_cast<u8*>(1), 1, 1};
		const bool lowered = Arm64Ir::LowerBoundVfConvertPlan(valid_plan, 3, 4, &out, &error,
			static_cast<Arm64Ir::BoundVfConvertLayout>(2));
		const bool no_code = before == oakGetCurrentCodePointer() && out.host_size == 0 &&
			out.frame_size == 0 && out.entry == nullptr;
		oakAsm->RET();
		oakEndBlock();
		Check(!lowered && no_code, "shared VU ITOF rejects unknown layout without emission");
	}
}

void VUSharedItofIntegrationTests()
{
	const auto itof = [](u32 fractional_bits, u32 fs, u32 ft, u32 mask) {
		u32 major = 0x3c;
		switch (fractional_bits)
		{
			case 0: major = 0x3c; break;
			case 4: major = 0x3d; break;
			case 12: major = 0x3e; break;
			case 15: major = 0x3f; break;
		}
		return major | (4u << 6) | (fs << 11) | (ft << 16) | ((mask & 15u) << 21);
	};
	const auto move = [](u32 fs, u32 ft, u32 mask) {
		return 0x8000033cu | (fs << 11) | (ft << 16) | ((mask & 15u) << 21);
	};
	const auto mr32 = [](u32 fs, u32 ft, u32 mask) {
		return 0x8000033du | (fs << 11) | (ft << 16) | ((mask & 15u) << 21);
	};
	constexpr std::array<u32, 4> fractions = {0, 4, 12, 15};
	constexpr std::array<VuItofRounding, 4> roundings = {
		VuItofRounding::NearestEven, VuItofRounding::PlusInfinity,
		VuItofRounding::MinusInfinity, VuItofRounding::TowardZero};
	constexpr std::array<FPRoundMode, 4> fpcr_modes = {
		FPRoundMode::Nearest, FPRoundMode::PositiveInfinity,
		FPRoundMode::NegativeInfinity, FPRoundMode::ChopZero};
	std::array<u32, 128> vf{};
	vf[3] = 0x3f800000u;
	for (u32 r = 1; r < 32; ++r)
		for (u32 lane = 0; lane < 4; ++lane)
			vf[r * 4 + lane] = 0x51000000u ^ (r * 0x01020408u) ^ (lane * 0x11111111u);
	constexpr std::array<u32, 4> destination = {0xdeadbeefu, 0xcafebabeu, 0x81234567u, 0x7fc54321u};
	for (u32 vu = 0; vu < 2; ++vu)
	{
		FPControlRegister& vu_fpcr = vu ? EmuConfig.Cpu.VU1FPCR : EmuConfig.Cpu.VU0FPCR;
		FPControlRegister& ee_fpcr = EmuConfig.Cpu.FPUFPCR;
		const FPControlRegister saved_vu_fpcr = vu_fpcr;
		const FPControlRegister saved_ee_fpcr = ee_fpcr;
		for (u32 fpcr_index = 0; fpcr_index < 4; ++fpcr_index)
		{
			// RunVU's JIT starts in the EE FPCR, while the interpreter installs
			// the per-VU FPCR. Keep both equal because the cached dispatcher may
			// have been emitted before either test value was selected.
			ee_fpcr.SetRoundMode(fpcr_modes[fpcr_index]);
			vu_fpcr.SetRoundMode(fpcr_modes[fpcr_index]);
			for (u32 fraction_index = 0; fraction_index < fractions.size(); ++fraction_index)
			for (u32 mask = 1; mask < 16; ++mask)
			{
				std::array<u32, 128> input = vf;
				for (u32 lane = 0; lane < 4; ++lane)
				{
					input[7 * 4 + lane] = kVuItofRawInputs[(mask * 3 + lane * 5 + fraction_index) % kVuItofRawInputs.size()];
					input[9 * 4 + lane] = destination[lane];
				}
				const std::vector<u32> program = {NOP_L,
					itof(fractions[fraction_index], 7, 9, mask), NOP_L,
					NOP_U | 0x40000000u, NOP_L, NOP_U};
				EmuCoreXOracleSetVUIR(vu, 0);
				const u64 before_legacy = EmuCoreXOracleVUIRItofInstructions(vu);
				const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, input.data());
				const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
				Check(EmuCoreXOracleVUIRItofInstructions(vu) == before_legacy,
					"legacy upper ITOF leaves common-IR counter unchanged");
				EmuCoreXOracleSetVUIR(vu, 1);
				const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
				Compare(interp, legacy, "ITOF interpreter and legacy JIT match at each FPCR mode");
				Compare(legacy, shared, "shared upper ITOF matches legacy state and memory");
				std::array<u32, 128> expected{};
				std::memcpy(expected.data(), input.data(), sizeof(expected));
				for (u32 lane = 0; lane < 4; ++lane)
					if (mask & (8u >> lane))
						expected[9 * 4 + lane] = VuItofExpectedBits(input[7 * 4 + lane],
							fractions[fraction_index], roundings[fpcr_index]);
				const bool exact = std::memcmp(interp.regs.VF, expected.data(), sizeof(expected)) == 0 &&
					std::memcmp(legacy.regs.VF, expected.data(), sizeof(expected)) == 0 &&
					std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0;
				Check(exact, "all three upper ITOF paths match independent integer-only VF expectation");
				Check(shared.regs.cycle == legacy.regs.cycle,
					"shared upper ITOF preserves exact legacy cycle count");
				Check(shared.regs.macflag == legacy.regs.macflag &&
					shared.regs.statusflag == legacy.regs.statusflag && shared.regs.clipflag == legacy.regs.clipflag,
					"shared upper ITOF preserves zero MAC, STATUS, and CLIP state");
				Check(EmuCoreXOracleVUIRItofInstructions(vu) == before_legacy + 1,
					"eligible upper ITOF routes through common IR on both VU cores");
			}
		}
		ee_fpcr.SetRoundMode(FPRoundMode::Nearest);
		vu_fpcr.SetRoundMode(FPRoundMode::Nearest);

		// Aliased source/destination exercises allocator source-before-writable binding.
		for (u32 mask : {15u, 4u})
		{
			std::array<u32, 128> input = vf;
			for (u32 lane = 0; lane < 4; ++lane)
				input[7 * 4 + lane] = kVuItofRawInputs[(lane + 8) % kVuItofRawInputs.size()];
			const std::vector<u32> program = {NOP_L, itof(12, 7, 7, mask), NOP_L,
				NOP_U | 0x40000000u, NOP_L, NOP_U};
			EmuCoreXOracleSetVUIR(vu, 0);
			const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, input.data());
			const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			const u64 before = EmuCoreXOracleVUIRItofInstructions(vu);
			EmuCoreXOracleSetVUIR(vu, 1);
			const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			Compare(legacy, shared, "aliased shared upper ITOF matches legacy");
			std::array<u32, 128> expected{};
			std::memcpy(expected.data(), input.data(), sizeof(expected));
			for (u32 lane = 0; lane < 4; ++lane)
				if (mask & (8u >> lane))
					expected[7 * 4 + lane] = VuItofExpectedBits(input[7 * 4 + lane], 12,
					VuItofRounding::NearestEven);
			Check(std::memcmp(interp.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				std::memcmp(legacy.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				shared.regs.cycle == legacy.regs.cycle &&
				EmuCoreXOracleVUIRItofInstructions(vu) == before + 1,
				"aliased ITOF preserves exact lanes, cycles, and shared counter");
		}

		// Source VF0 and no-op boundaries exercise architectural constants and scheduler admission.
		for (const auto [fs, ft, mask, should_route] : {
			std::array<u32, 4>{0, 9, 15, 1}, std::array<u32, 4>{7, 0, 15, 0},
			std::array<u32, 4>{7, 9, 0, 0}})
		{
			std::array<u32, 128> input = vf;
			const std::vector<u32> program = {NOP_L, itof(4, fs, ft, mask), NOP_L,
				NOP_U | 0x40000000u, NOP_L, NOP_U};
			EmuCoreXOracleSetVUIR(vu, 0);
			const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, input.data());
			const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			const u64 before = EmuCoreXOracleVUIRItofInstructions(vu);
			EmuCoreXOracleSetVUIR(vu, 1);
			const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			Compare(interp, shared, "ITOF VF0 and empty-mask state matches interpreter");
			Compare(legacy, shared, "ITOF VF0 and empty-mask state matches legacy");
			Check(shared.regs.cycle == legacy.regs.cycle &&
				EmuCoreXOracleVUIRItofInstructions(vu) == before + (should_route ? 1u : 0u),
				"ITOF VF0/mask boundary preserves cycles and routing admission");
			std::array<u32, 128> expected{};
			std::memcpy(expected.data(), input.data(), sizeof(expected));
			if (fs == 0 && should_route)
				for (u32 lane = 0; lane < 4; ++lane)
					expected[ft * 4 + lane] = VuItofExpectedBits(input[lane], 4,
						VuItofRounding::NearestEven);
			Check(std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0,
				"ITOF VF0/mask boundary matches independent raw integer VF expectation");
		}

		// The upper conversion wins a same-pair WAW against lower MR32.
		{
			std::array<u32, 128> input = vf;
			const std::vector<u32> program = {mr32(6, 9, 15), itof(15, 7, 9, 5), NOP_L,
				NOP_U | 0x40000000u, NOP_L, NOP_U};
			EmuCoreXOracleSetVUIR(vu, 0);
			const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			const u64 before = EmuCoreXOracleVUIRItofInstructions(vu);
			EmuCoreXOracleSetVUIR(vu, 1);
			const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			Compare(legacy, shared, "same-pair upper ITOF preserves lower/upper write priority");
			std::array<u32, 128> expected{};
			std::memcpy(expected.data(), input.data(), sizeof(expected));
			for (u32 lane = 0; lane < 4; ++lane)
				if (5u & (8u >> lane))
					expected[9 * 4 + lane] = VuItofExpectedBits(input[7 * 4 + lane], 15,
						VuItofRounding::NearestEven);
			Check(std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				shared.regs.cycle == legacy.regs.cycle && EmuCoreXOracleVUIRItofInstructions(vu) == before + 1,
				"same-pair ITOF matches independent expectation and exact cycles");
		}

		// A lower MOVE producer in the preceding pair must feed the raw conversion.
		{
			std::array<u32, 128> input = vf;
			const std::vector<u32> program = {move(5, 7, 15), NOP_U, NOP_L,
				itof(4, 7, 9, 15), NOP_L, NOP_U | 0x40000000u, NOP_L, NOP_U};
			EmuCoreXOracleSetVUIR(vu, 0);
			const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, input.data());
			const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			const u64 before = EmuCoreXOracleVUIRItofInstructions(vu);
			EmuCoreXOracleSetVUIR(vu, 1);
			const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			Compare(interp, shared, "lower MOVE producer feeds upper ITOF");
			Compare(legacy, shared, "dirty lower MOVE source ITOF matches legacy");
			std::array<u32, 128> expected{};
			std::memcpy(expected.data(), input.data(), sizeof(expected));
			for (u32 lane = 0; lane < 4; ++lane)
				expected[7 * 4 + lane] = input[5 * 4 + lane];
			for (u32 lane = 0; lane < 4; ++lane)
				expected[9 * 4 + lane] = VuItofExpectedBits(expected[7 * 4 + lane], 4,
					VuItofRounding::NearestEven);
			Check(std::memcmp(interp.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				std::memcmp(legacy.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				shared.regs.cycle == legacy.regs.cycle && EmuCoreXOracleVUIRItofInstructions(vu) == before + 1,
				"lower producer ITOF matches independent expectation and exact cycles");
		}

		// An actual branch delay slot must keep pass1's delay flag and exact timing.
		{
			const u32 branch = (0x28u << 25) | (1u << 11) | (2u << 16) | 2u;
			const std::vector<u32> program = {branch, NOP_U, NOP_L, itof(12, 7, 9, 10),
				NOP_L, NOP_U, NOP_L, NOP_U | 0x40000000u, NOP_L, NOP_U};
			std::array<u32, 16> vi{};
			EmuCoreXOracleSetVUIR(vu, 0);
			const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, vf.data(), nullptr, vi.data());
			const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, vf.data(), nullptr, vi.data());
			const u64 before = EmuCoreXOracleVUIRItofInstructions(vu);
			EmuCoreXOracleSetVUIR(vu, 1);
			const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, vf.data(), nullptr, vi.data());
			Compare(interp, legacy, "branch-delay upper ITOF interpreter/legacy parity");
			Compare(legacy, shared, "branch-delay ITOF preserves full state");
			std::array<u32, 128> expected{};
			std::memcpy(expected.data(), vf.data(), sizeof(expected));
			for (u32 lane = 0; lane < 4; ++lane)
				if (10u & (8u >> lane))
					expected[9 * 4 + lane] = VuItofExpectedBits(vf[7 * 4 + lane], 12,
						VuItofRounding::NearestEven);
			Check(std::memcmp(interp.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				std::memcmp(legacy.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				shared.regs.cycle == legacy.regs.cycle && EmuCoreXOracleVUIRItofInstructions(vu) == before + 1,
				"branch-delay ITOF retains pass1 scheduling and exact cycles");
		}
		ee_fpcr = saved_ee_fpcr;
		vu_fpcr = saved_vu_fpcr;
		EmuCoreXOracleSetVUIR(vu, -1);
	}
}

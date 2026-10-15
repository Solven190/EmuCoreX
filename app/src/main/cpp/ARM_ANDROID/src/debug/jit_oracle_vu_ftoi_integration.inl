// SPDX-License-Identifier: GPL-3.0+
// Production-integration oracle for real VU upper FTOI0/4/12/15.
// The raw integer expected model is shared with jit_oracle_vu_ftoi.inl.
static u32 VuFtoiExpectedBits(u32 raw_bits, u32 fractional_bits, u32 fpcr_bits);
extern "C" u64 EmuCoreXOracleVUIRFtoiInstructions(u32 vu);

void VUSharedFtoiIntegrationTests()
{
	const auto ftoi = [](u32 fractional_bits, u32 fs, u32 ft, u32 mask) {
		u32 major = 0x3c;
		switch (fractional_bits)
		{
			case 0: major = 0x3c; break;
			case 4: major = 0x3d; break;
			case 12: major = 0x3e; break;
			case 15: major = 0x3f; break;
		}
		return major | (5u << 6) | (fs << 11) | (ft << 16) | ((mask & 15u) << 21);
	};
	const auto move = [](u32 fs, u32 ft, u32 mask) {
		return 0x8000033cu | (fs << 11) | (ft << 16) | ((mask & 15u) << 21);
	};
	const auto mr32 = [](u32 fs, u32 ft, u32 mask) {
		return 0x8000033du | (fs << 11) | (ft << 16) | ((mask & 15u) << 21);
	};
	const auto upperAdd = [](u32 fs, u32 ft, u32 fd, u32 mask) {
		return 0x28u | (fd << 6) | (fs << 11) | (ft << 16) | ((mask & 15u) << 21);
	};
	const auto upperClip = [](u32 fs, u32 ft) {
		return 0x3fu | (7u << 6) | (fs << 11) | (ft << 16);
	};
	constexpr std::array<u32, 4> fractions = {0, 4, 12, 15};
	constexpr std::array<FPRoundMode, 4> round_modes = {
		FPRoundMode::Nearest, FPRoundMode::PositiveInfinity,
		FPRoundMode::NegativeInfinity, FPRoundMode::ChopZero};
	constexpr std::array<u32, 16> edge_bits = {
		0x00000000u, 0x80000000u, 0x3f000000u, 0xbf400000u,
		0x3f800000u, 0xbf800000u, 0x4effffffu, 0xceffffffu,
		0x4f000000u, 0xcf000000u, 0x4f000001u, 0xcf000001u,
		0x00000001u, 0x807fffffu, 0x7f800000u, 0xff800000u};
	constexpr std::array<u32, 4> special_nan_bits = {
		0x7fc12345u, 0xffc54321u, 0x7f812345u, 0xff854321u};
	constexpr std::array<u32, 4> initial_destination = {
		0xdeadbeefu, 0xcafebabeu, 0x81234567u, 0x7fc54321u};

	std::array<u32, 128> initial_vf{};
	initial_vf[3] = 0x3f800000u; // VF0 is +1.0 in all lanes.
	for (u32 r = 1; r < 32; ++r)
		for (u32 lane = 0; lane < 4; ++lane)
			initial_vf[r * 4 + lane] = 0x51000000u ^ (r * 0x01020408u) ^ (lane * 0x11111111u);
	for (u32 lane = 0; lane < 4; ++lane)
		initial_vf[9 * 4 + lane] = initial_destination[lane];

	for (u32 vu = 0; vu < 2; ++vu)
	{
		FPControlRegister& vu_fpcr = vu ? EmuConfig.Cpu.VU1FPCR : EmuConfig.Cpu.VU0FPCR;
		FPControlRegister& ee_fpcr = EmuConfig.Cpu.FPUFPCR;
		const FPControlRegister saved_vu_fpcr = vu_fpcr;
		const FPControlRegister saved_ee_fpcr = ee_fpcr;
		// Default-NaN is not modeled as a configurable VU mode in this fixture.
		constexpr u64 kFpcrDefaultNaN = 1ull << 25;
		ee_fpcr.bitmask = static_cast<decltype(ee_fpcr.bitmask)>(ee_fpcr.bitmask &
			~static_cast<decltype(ee_fpcr.bitmask)>(kFpcrDefaultNaN));
		vu_fpcr.bitmask = static_cast<decltype(vu_fpcr.bitmask)>(vu_fpcr.bitmask &
			~static_cast<decltype(vu_fpcr.bitmask)>(kFpcrDefaultNaN));

		// Every offset and every nonempty destination mask. Rotated edge inputs
		// cover signed zero, fractions, the В±2^31 boundary, subnormals, and infinities.
		for (u32 fraction_index = 0; fraction_index < fractions.size(); ++fraction_index)
		for (u32 mask = 1; mask < 16; ++mask)
		{
			vu_fpcr.SetRoundMode(FPRoundMode::Nearest);
			ee_fpcr = vu_fpcr;
			std::array<u32, 128> input = initial_vf;
			for (u32 lane = 0; lane < 4; ++lane)
			{
				input[7 * 4 + lane] = edge_bits[(mask + lane * 5 + fraction_index * 3) % edge_bits.size()];
				if ((fraction_index + mask + lane) % 5 == 0)
					input[7 * 4 + lane] = special_nan_bits[(lane + fraction_index) % special_nan_bits.size()];
			}
			const std::vector<u32> program = {NOP_L, ftoi(fractions[fraction_index], 7, 9, mask), NOP_L,
				NOP_U | 0x40000000u, NOP_L, NOP_U};

			EmuCoreXOracleSetVUIR(vu, 0);
			const u64 before = EmuCoreXOracleVUIRFtoiInstructions(vu);
			const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, input.data());
			const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			Check(EmuCoreXOracleVUIRFtoiInstructions(vu) == before,
				"forced-legacy VU FTOI leaves shared-IR counter unchanged");
			EmuCoreXOracleSetVUIR(vu, 1);
			const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			Compare(interp, legacy, "VU FTOI interpreter/legacy state parity");
			Compare(legacy, shared, "shared VU FTOI matches legacy state and memory");

			std::array<u32, 128> expected{};
			std::memcpy(expected.data(), input.data(), sizeof(expected));
			for (u32 lane = 0; lane < 4; ++lane)
				if (mask & (8u >> lane))
					expected[9 * 4 + lane] = VuFtoiExpectedBits(input[7 * 4 + lane],
						fractions[fraction_index], static_cast<u32>(vu_fpcr.bitmask));
			Check(std::memcmp(interp.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				std::memcmp(legacy.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0,
				"interpreter, legacy, and shared FTOI match raw integer expected model and preserve masked lanes");
			Check(shared.regs.cycle == legacy.regs.cycle,
				"shared upper FTOI preserves exact legacy cycles");
			Check(shared.regs.macflag == legacy.regs.macflag &&
				shared.regs.statusflag == legacy.regs.statusflag && shared.regs.clipflag == legacy.regs.clipflag,
				"shared upper FTOI preserves MAC, STATUS, and CLIP state");
			Check(EmuCoreXOracleVUIRFtoiInstructions(vu) == before + 1,
				"eligible upper FTOI routes through common IR on both VU cores");
		}

		// Exercise the positive and negative saturation thresholds and their
		// adjacent representable inputs for every scale, not just FTOI0.
		constexpr std::array<u32, 4> scaled_threshold_bits = {
			0x4f000000u, 0x4d000000u, 0x49000000u, 0x47800000u};
		for (u32 fraction_index = 0; fraction_index < fractions.size(); ++fraction_index)
		{
			vu_fpcr.SetRoundMode(FPRoundMode::Nearest);
			vu_fpcr.bitmask = static_cast<decltype(vu_fpcr.bitmask)>(vu_fpcr.bitmask &
				~static_cast<decltype(vu_fpcr.bitmask)>(kFpcrDefaultNaN));
			ee_fpcr = vu_fpcr;
			const u32 threshold = scaled_threshold_bits[fraction_index];
			for (u32 adjacent = 0; adjacent < 2; ++adjacent)
			{
				std::array<u32, 128> input = initial_vf;
				const std::array<u32, 4> raw = adjacent == 0 ?
					std::array<u32, 4>{threshold - 1, threshold, 0x80000000u | threshold,
						0x80000000u | (threshold - 1)} :
					std::array<u32, 4>{threshold + 1, 0x80000000u | (threshold + 1),
						0x80000000u | (threshold - 1), 0x80000000u | threshold};
				for (u32 lane = 0; lane < 4; ++lane)
					input[7 * 4 + lane] = raw[lane];
				const std::vector<u32> program = {NOP_L, ftoi(fractions[fraction_index], 7, 9, 15), NOP_L,
					NOP_U | 0x40000000u, NOP_L, NOP_U};
				EmuCoreXOracleSetVUIR(vu, 0);
				const u64 before = EmuCoreXOracleVUIRFtoiInstructions(vu);
				const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, input.data());
				const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
				EmuCoreXOracleSetVUIR(vu, 1);
				const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
				std::array<u32, 128> expected{};
				std::memcpy(expected.data(), input.data(), sizeof(expected));
				for (u32 lane = 0; lane < 4; ++lane)
					expected[9 * 4 + lane] = VuFtoiExpectedBits(raw[lane], fractions[fraction_index],
						static_cast<u32>(vu_fpcr.bitmask));
				Check(std::memcmp(interp.regs.VF, expected.data(), sizeof(expected)) == 0 &&
					std::memcmp(legacy.regs.VF, expected.data(), sizeof(expected)) == 0 &&
					std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0 &&
					legacy.regs.cycle == shared.regs.cycle &&
					EmuCoreXOracleVUIRFtoiInstructions(vu) == before + 1,
					"FTOI all-scale saturation thresholds match interpreter, legacy, and shared IR");
			}
		}

		// Rounding mode and FZ/DAZ do not change integer results for power-of-two
		// scaling, but these cases lock down active per-VU FPCR integration.
		for (u32 fraction_index = 0; fraction_index < fractions.size(); ++fraction_index)
		for (u32 rounding = 0; rounding < round_modes.size(); ++rounding)
		for (u32 flush = 0; flush < 2; ++flush)
		{
			vu_fpcr.SetRoundMode(round_modes[rounding]);
			vu_fpcr = vu_fpcr.SetDenormalsAreZero(flush != 0);
			vu_fpcr = vu_fpcr.SetFlushToZero(flush != 0);
			ee_fpcr = vu_fpcr;
			std::array<u32, 128> input = initial_vf;
			input[7 * 4 + 0] = 0xff812345u; // scaled negative sNaN; default-NaN is clear.
			input[7 * 4 + 1] = 0x7fc54321u;
			input[7 * 4 + 2] = 0x00000001u;
			input[7 * 4 + 3] = 0x4effffffu;
			const std::vector<u32> program = {NOP_L, ftoi(fractions[fraction_index], 7, 9, 15), NOP_L,
				NOP_U | 0x40000000u, NOP_L, NOP_U};
			EmuCoreXOracleSetVUIR(vu, 0);
			const u64 before = EmuCoreXOracleVUIRFtoiInstructions(vu);
			const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, input.data());
			const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			EmuCoreXOracleSetVUIR(vu, 1);
			const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			Compare(interp, shared, "VU FTOI honors per-core FPCR in interpreter/shared integration");
			Compare(legacy, shared, "VU FTOI FPCR cases match legacy");
			std::array<u32, 128> expected{};
			std::memcpy(expected.data(), input.data(), sizeof(expected));
			for (u32 lane = 0; lane < 4; ++lane)
				expected[9 * 4 + lane] = VuFtoiExpectedBits(input[7 * 4 + lane], fractions[fraction_index],
					static_cast<u32>(vu_fpcr.bitmask));
			Check(std::memcmp(interp.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				std::memcmp(legacy.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				shared.regs.cycle == legacy.regs.cycle &&
				EmuCoreXOracleVUIRFtoiInstructions(vu) == before + 1,
				"all FPCR modes preserve FTOI raw integer results, exact cycles, and admission counter");
		}

		// Scaled NaNs observe FPCR.DN through the legacy FMUL step. FTOI0 has
		// no scale operation and therefore keeps the source NaN sign; FTOI4/12/15
		// receive the positive default NaN before integer saturation.
		for (u32 fraction_index = 0; fraction_index < fractions.size(); ++fraction_index)
		{
			vu_fpcr.bitmask = static_cast<decltype(vu_fpcr.bitmask)>(vu_fpcr.bitmask |
				static_cast<decltype(vu_fpcr.bitmask)>(kFpcrDefaultNaN));
			ee_fpcr = vu_fpcr;
			std::array<u32, 128> input = initial_vf;
			input[7 * 4 + 0] = 0xff812345u; // negative sNaN
			input[7 * 4 + 1] = 0xffc54321u; // negative qNaN
			input[7 * 4 + 2] = 0x7f812345u;  // positive sNaN
			input[7 * 4 + 3] = 0x7fc54321u;  // positive qNaN
			const std::vector<u32> program = {NOP_L, ftoi(fractions[fraction_index], 7, 9, 15), NOP_L,
				NOP_U | 0x40000000u, NOP_L, NOP_U};
			EmuCoreXOracleSetVUIR(vu, 0);
			const u64 before = EmuCoreXOracleVUIRFtoiInstructions(vu);
			const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, input.data());
			const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			EmuCoreXOracleSetVUIR(vu, 1);
			const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			Compare(interp, legacy, "DN FTOI interpreter/legacy parity");
			Compare(legacy, shared, "DN FTOI shared/legacy parity");
			std::array<u32, 128> expected{};
			std::memcpy(expected.data(), input.data(), sizeof(expected));
			for (u32 lane = 0; lane < 4; ++lane)
				expected[9 * 4 + lane] = VuFtoiExpectedBits(input[7 * 4 + lane], fractions[fraction_index],
					static_cast<u32>(vu_fpcr.bitmask));
			Check(std::memcmp(interp.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				std::memcmp(legacy.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				legacy.regs.cycle == shared.regs.cycle &&
				EmuCoreXOracleVUIRFtoiInstructions(vu) == before + 1,
				"FPCR.DN controls negative and positive NaN saturation for all FTOI scales");
		}

		ee_fpcr = saved_ee_fpcr;
		vu_fpcr = saved_vu_fpcr;

		// Fs==Ft must retain the input before a full or partial masked write.
		for (u32 mask : {15u, 4u})
		{
			std::array<u32, 128> input = initial_vf;
			for (u32 lane = 0; lane < 4; ++lane)
				input[7 * 4 + lane] = edge_bits[(lane * 3 + 7) % edge_bits.size()];
			const std::vector<u32> program = {NOP_L, ftoi(12, 7, 7, mask), NOP_L,
				NOP_U | 0x40000000u, NOP_L, NOP_U};
			EmuCoreXOracleSetVUIR(vu, 0);
			const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, input.data());
			const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			const u64 before = EmuCoreXOracleVUIRFtoiInstructions(vu);
			EmuCoreXOracleSetVUIR(vu, 1);
			const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			Compare(interp, legacy, "aliased VU FTOI interpreter/legacy parity");
			Compare(legacy, shared, "aliased shared VU FTOI matches legacy");
			std::array<u32, 128> expected{};
			std::memcpy(expected.data(), input.data(), sizeof(expected));
			for (u32 lane = 0; lane < 4; ++lane)
				if (mask & (8u >> lane))
					expected[7 * 4 + lane] = VuFtoiExpectedBits(input[7 * 4 + lane], 12,
						static_cast<u32>(vu_fpcr.bitmask));
			Check(std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				shared.regs.cycle == legacy.regs.cycle && EmuCoreXOracleVUIRFtoiInstructions(vu) == before + 1,
				"aliased FTOI preserves raw lanes, masked neighbors, exact cycles, and shared admission");
		}

		// VF0 is (0,0,0,1); a VF0 destination and empty mask remain no-ops.
		for (const auto [fs, ft, mask, should_route] : {
			std::array<u32, 4>{0, 9, 15, 1}, std::array<u32, 4>{7, 0, 15, 0},
			std::array<u32, 4>{7, 9, 0, 0}})
		{
			const std::array<u32, 128> input = initial_vf;
			const std::vector<u32> program = {NOP_L, ftoi(4, fs, ft, mask), NOP_L,
				NOP_U | 0x40000000u, NOP_L, NOP_U};
			EmuCoreXOracleSetVUIR(vu, 0);
			const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, input.data());
			const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			const u64 before = EmuCoreXOracleVUIRFtoiInstructions(vu);
			EmuCoreXOracleSetVUIR(vu, 1);
			const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			Compare(interp, legacy, "FTOI VF0/mask boundary interpreter/legacy parity");
			Compare(legacy, shared, "FTOI VF0/mask boundary shared/legacy parity");
			std::array<u32, 128> expected = initial_vf;
			if (should_route)
				for (u32 lane = 0; lane < 4; ++lane)
					expected[9 * 4 + lane] = VuFtoiExpectedBits(input[fs * 4 + lane], 4,
						static_cast<u32>(vu_fpcr.bitmask));
			Check(std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				shared.regs.cycle == legacy.regs.cycle &&
				EmuCoreXOracleVUIRFtoiInstructions(vu) == before + (should_route ? 1u : 0u),
				"FTOI VF0 source, VF0 destination, and zero mask preserve exact state and admission");
		}

		// Lower MOVE producer must be visible to the following upper FTOI.
		{
			std::array<u32, 128> input = initial_vf;
			for (u32 lane = 0; lane < 4; ++lane)
				input[7 * 4 + lane] = edge_bits[(lane + 2) % edge_bits.size()];
			const std::vector<u32> program = {move(7, 8, 15), NOP_U, NOP_L,
				ftoi(15, 8, 9, 15), NOP_L, NOP_U | 0x40000000u, NOP_L, NOP_U};
			EmuCoreXOracleSetVUIR(vu, 0);
			const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, input.data());
			const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			const u64 before = EmuCoreXOracleVUIRFtoiInstructions(vu);
			EmuCoreXOracleSetVUIR(vu, 1);
			const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			Compare(interp, legacy, "FTOI lower-producer interpreter/legacy parity");
			Compare(legacy, shared, "FTOI sees preceding lower MOVE producer");
			std::array<u32, 128> expected{};
			std::memcpy(expected.data(), input.data(), sizeof(expected));
			for (u32 lane = 0; lane < 4; ++lane)
			{
				expected[8 * 4 + lane] = input[7 * 4 + lane];
				expected[9 * 4 + lane] = VuFtoiExpectedBits(expected[8 * 4 + lane], 15,
					static_cast<u32>(vu_fpcr.bitmask));
			}
			Check(std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				shared.regs.cycle == legacy.regs.cycle && EmuCoreXOracleVUIRFtoiInstructions(vu) == before + 1,
				"FTOI producer-consumer checks raw result, cycles, and admission");
		}

		// Upper FTOI wins same-pair WAW against lower MR32.
		{
			std::array<u32, 128> input = initial_vf;
			for (u32 lane = 0; lane < 4; ++lane)
				input[7 * 4 + lane] = edge_bits[(lane + 9) % edge_bits.size()];
			const std::vector<u32> program = {mr32(6, 9, 15), ftoi(12, 7, 9, 5), NOP_L,
				NOP_U | 0x40000000u, NOP_L, NOP_U};
			EmuCoreXOracleSetVUIR(vu, 0);
			const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			const u64 before = EmuCoreXOracleVUIRFtoiInstructions(vu);
			EmuCoreXOracleSetVUIR(vu, 1);
			const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, input.data());
			Compare(legacy, shared, "FTOI same-pair WAW preserves legacy upper priority");
			std::array<u32, 128> expected{};
			std::memcpy(expected.data(), input.data(), sizeof(expected));
			for (u32 lane = 0; lane < 4; ++lane)
				if (5u & (8u >> lane))
					expected[9 * 4 + lane] = VuFtoiExpectedBits(input[7 * 4 + lane], 12,
						static_cast<u32>(vu_fpcr.bitmask));
			Check(std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				shared.regs.cycle == legacy.regs.cycle && EmuCoreXOracleVUIRFtoiInstructions(vu) == before + 1,
				"FTOI same-pair WAW preserves exact expected lanes and cycles");
		}

		// The delay-slot FTOI is routed through shared upper lowering with legacy cycles.
		{
			std::array<u32, 128> input = initial_vf;
			const u32 branch = (0x28u << 25) | (1u << 11) | (2u << 16) | 2u;
			std::array<u32, 16> vi{};
			const std::vector<u32> program = {branch, NOP_U, NOP_L, ftoi(4, 7, 9, 10),
				NOP_L, NOP_U, NOP_L, NOP_U | 0x40000000u, NOP_L, NOP_U};
			EmuCoreXOracleSetVUIR(vu, 0);
			const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false,
				nullptr, input.data(), nullptr, vi.data());
			const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false,
				nullptr, input.data(), nullptr, vi.data());
			const u64 before = EmuCoreXOracleVUIRFtoiInstructions(vu);
			EmuCoreXOracleSetVUIR(vu, 1);
			const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false,
				nullptr, input.data(), nullptr, vi.data());
			Compare(interp, legacy, "branch-delay FTOI interpreter/legacy parity");
			Compare(legacy, shared, "branch-delay shared FTOI matches legacy");
			std::array<u32, 128> expected{};
			std::memcpy(expected.data(), input.data(), sizeof(expected));
			for (u32 lane = 0; lane < 4; ++lane)
				if (10u & (8u >> lane))
					expected[9 * 4 + lane] = VuFtoiExpectedBits(input[7 * 4 + lane], 4,
						static_cast<u32>(vu_fpcr.bitmask));
			Check(std::memcmp(shared.regs.VF, expected.data(), sizeof(expected)) == 0 &&
				shared.regs.cycle == legacy.regs.cycle && EmuCoreXOracleVUIRFtoiInstructions(vu) == before + 1,
				"branch-delay FTOI preserves raw result, exact cycles, and shared admission");
		}

		// Seed nonzero MAC/STATUS/CLIP flags and ACC through legacy VU operations,
		// then resume into FTOI. The conversion itself must leave all four intact.
		{
			std::array<u32, 128> input = initial_vf;
			input[7 * 4 + 0] = 0x40000000u;
			input[7 * 4 + 1] = 0xc0000000u;
			input[7 * 4 + 2] = 0x40000000u;
			input[7 * 4 + 3] = 0xc0000000u;
			input[8 * 4 + 0] = 0xbf800000u;
			input[8 * 4 + 1] = 0xbf800000u;
			input[8 * 4 + 2] = 0xbf800000u;
			input[8 * 4 + 3] = 0x3f800000u; // CLIP threshold is Ft.w = +1.
			const std::array<u32, 4> seed_acc = {0x3fa00000u, 0x40200000u, 0xc0700000u, 0x40900000u};
			const std::vector<u32> seed_program = {NOP_L, upperAdd(7, 8, 12, 15),
				NOP_L, upperClip(7, 8), NOP_L, NOP_U, NOP_L, NOP_U,
				NOP_L, NOP_U, NOP_L, NOP_U, NOP_L, NOP_U | 0x40000000u};
			const std::vector<u32> convert_program = {NOP_L, ftoi(4, 7, 10, 15), NOP_L,
				NOP_U | 0x40000000u, NOP_L, NOP_U};
			std::array<Snapshot, 3> seeded_result{};
			std::array<Snapshot, 3> result{};
			for (u32 route = 0; route < result.size(); ++route)
			{
				EmuCoreXOracleSetVUIR(vu, route == 2 ? 1 : 0);
				const bool jit = route != 0;
				const u64 before = EmuCoreXOracleVUIRFtoiInstructions(vu);
				const Snapshot seeded = RunVU(vu, jit, seed_program, 0x80, 0, 128, false,
					nullptr, input.data(), seed_acc.data());
				seeded_result[route] = seeded;
				std::printf("FTOISEED core=%u route=%u aux_mac=%08x aux_status=%08x aux_clip=%08x vi_mac=%08x vi_status=%08x vi_clip=%08x acc=%08x,%08x,%08x,%08x\n",
					vu, route, seeded.regs.macflag, seeded.regs.statusflag, seeded.regs.clipflag,
					seeded.regs.VI[REG_MAC_FLAG].UL, seeded.regs.VI[REG_STATUS_FLAG].UL,
					seeded.regs.VI[REG_CLIP_FLAG].UL, seeded.regs.ACC.UL[0], seeded.regs.ACC.UL[1],
					seeded.regs.ACC.UL[2], seeded.regs.ACC.UL[3]);
				const bool flags_seeded = seeded.regs.VI[REG_MAC_FLAG].UL != 0 &&
					seeded.regs.VI[REG_STATUS_FLAG].UL != 0 && seeded.regs.VI[REG_CLIP_FLAG].UL != 0 &&
					std::memcmp(&seeded.regs.ACC, seed_acc.data(), sizeof(seed_acc)) == 0;
				Check(flags_seeded, "FTOI flag-preservation fixture seeds nonzero MAC/STATUS/CLIP and ACC");
				result[route] = RunVU(vu, jit, convert_program, 0x100, 0, 128, true);
				if (route != 2)
					Check(EmuCoreXOracleVUIRFtoiInstructions(vu) == before,
						"interpreter/legacy seeded FTOI does not enter shared-IR lowering");
				else
					Check(EmuCoreXOracleVUIRFtoiInstructions(vu) == before + 1,
						"shared seeded FTOI increments the production admission counter");
			}
			Compare(result[0], result[1], "seeded-flag FTOI interpreter/legacy parity");
			Compare(result[1], result[2], "seeded-flag shared FTOI preserves legacy state");
			for (u32 route = 0; route < result.size(); ++route)
				Check(result[route].regs.VI[REG_MAC_FLAG].UL == seeded_result[route].regs.VI[REG_MAC_FLAG].UL &&
					result[route].regs.VI[REG_STATUS_FLAG].UL == seeded_result[route].regs.VI[REG_STATUS_FLAG].UL &&
					result[route].regs.VI[REG_CLIP_FLAG].UL == seeded_result[route].regs.VI[REG_CLIP_FLAG].UL &&
					std::memcmp(&result[route].regs.ACC, &seeded_result[route].regs.ACC, sizeof(seed_acc)) == 0,
					"FTOI leaves seeded architectural MAC/STATUS/CLIP VI flags and ACC exactly unchanged");
			std::array<u32, 128> expected{};
			std::memcpy(expected.data(), input.data(), sizeof(expected));
			// Independent finite-lane model for the seed ADD: [2,-2,2,-2] +
			// [-1,-1,-1,1] -> [1,-3,1,-1] in VF12.
			constexpr std::array<u32, 4> seeded_add_result = {
				0x3f800000u, 0xc0400000u, 0x3f800000u, 0xbf800000u};
			for (u32 lane = 0; lane < 4; ++lane)
				expected[12 * 4 + lane] = seeded_add_result[lane];
			for (u32 lane = 0; lane < 4; ++lane)
				expected[10 * 4 + lane] = VuFtoiExpectedBits(input[7 * 4 + lane], 4,
						static_cast<u32>(vu_fpcr.bitmask));
			for (const Snapshot& snapshot : result)
				Check(std::memcmp(snapshot.regs.VF, expected.data(), sizeof(expected)) == 0,
					"seeded FTOI preserves flags/ACC and matches independent raw integer output");
			Check(result[1].regs.cycle == result[2].regs.cycle,
				"seeded shared FTOI preserves exact legacy cycle count");
		}

		// Reset the per-core override and restore caller FPCRs before the next core.
		EmuCoreXOracleSetVUIR(vu, -1);
		ee_fpcr = saved_ee_fpcr;
		vu_fpcr = saved_vu_fpcr;
	}
}

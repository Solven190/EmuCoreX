// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+
// EE COP2 macro integration tests for shared upper VU FTOI.

static u32 VuFtoiExpectedBits(u32 raw_bits, u32 fractional_bits, u32 fpcr_bits);
extern "C" u64 EmuCoreXOracleVUIRFtoiInstructions(u32 vu);

void EEVuFtoiMacroTests()
{
	const auto original_ee_fpcr = EmuConfig.Cpu.FPUFPCR;
	const auto original_vu0_fpcr = EmuConfig.Cpu.VU0FPCR;
	EmuConfig.Cpu.FPUFPCR.SetRoundMode(FPRoundMode::Nearest);
	EmuConfig.Cpu.VU0FPCR.SetRoundMode(FPRoundMode::Nearest);
	constexpr u64 kFpcrDefaultNaN = 1ull << 25;
	EmuConfig.Cpu.FPUFPCR.bitmask = static_cast<decltype(EmuConfig.Cpu.FPUFPCR.bitmask)>(
		EmuConfig.Cpu.FPUFPCR.bitmask & ~static_cast<decltype(EmuConfig.Cpu.FPUFPCR.bitmask)>(kFpcrDefaultNaN));
	EmuConfig.Cpu.VU0FPCR.bitmask = static_cast<decltype(EmuConfig.Cpu.VU0FPCR.bitmask)>(
		EmuConfig.Cpu.VU0FPCR.bitmask & ~static_cast<decltype(EmuConfig.Cpu.VU0FPCR.bitmask)>(kFpcrDefaultNaN));
	const FPControlRegisterBackup host_fpcr(EmuConfig.Cpu.FPUFPCR);
	const auto qmtc2 = [](u32 rt, u32 vd) {
		return (0x12u << 26) | (5u << 21) | (rt << 16) | (vd << 11);
	};
	const auto qmfc2 = [](u32 rt, u32 vd) {
		return (0x12u << 26) | (1u << 21) | (rt << 16) | (vd << 11);
	};
	const auto ftoi = [](u32 fraction_index, u32 mask, u32 fs, u32 ft) {
		constexpr std::array<u32, 4> major = {0x3c, 0x3d, 0x3e, 0x3f};
		return (0x12u << 26) | ((0x10u | mask) << 21) | (ft << 16) | (fs << 11) |
			(5u << 6) | major[fraction_index];
	};
	constexpr std::array<u32, 4> fractions = {0, 4, 12, 15};
	constexpr std::array<u32, 4> source = {0x01000001u, 0xfeffffffu, 0x80000000u, 0x7fffffffu};
	constexpr std::array<u32, 4> initial_dest = {0xdeadbeefu, 0xcafebabeu, 0x81234567u, 0x7fc54321u};
	std::array<u32, 128> initial_vf{};
	initial_vf[3] = 0x3f800000u; // VF0 is immutable +1.0 in lane W.
	for (u32 reg = 1; reg < 32; ++reg)
		for (u32 lane = 0; lane < 4; ++lane)
			initial_vf[reg * 4 + lane] = 0x51000000u ^ (reg * 0x01020408u) ^ (lane * 0x11111111u);
	for (u32 lane = 0; lane < 4; ++lane)
		initial_vf[9 * 4 + lane] = initial_dest[lane];
	std::array<GPR_reg, 32> initial_gpr{};
	initial_gpr[8].UD[0] = static_cast<u64>(source[0]) | (static_cast<u64>(source[1]) << 32);
	initial_gpr[8].UD[1] = static_cast<u64>(source[2]) | (static_cast<u64>(source[3]) << 32);
	initial_gpr[10].UD[0] = static_cast<u64>(initial_dest[0]) | (static_cast<u64>(initial_dest[1]) << 32);
	initial_gpr[10].UD[1] = static_cast<u64>(initial_dest[2]) | (static_cast<u64>(initial_dest[3]) << 32);
	const auto current_flags = [] { return std::array<u32, 3>{VU0.macflag, VU0.statusflag, VU0.clipflag}; };
	const auto run_and_check = [&](const std::vector<u32>& code, const std::array<u32, 128>& expected_vf,
		const std::array<u32, 4>& expected_output, u32 routed, const char* label) {
		const u64 output_low = static_cast<u64>(expected_output[0]) |
			(static_cast<u64>(expected_output[1]) << 32);
		const u64 output_high = static_cast<u64>(expected_output[2]) |
			(static_cast<u64>(expected_output[3]) << 32);
		EmuCoreXOracleSetEeIrMode(0);
		EmuCoreXOracleSetVUIR(0, 0);
		const u64 before_legacy = EmuCoreXOracleVUIRFtoiInstructions(0);
		const EESnapshot legacy = RunEEProgramWithIrMode(0, code, initial_gpr.data(), nullptr,
			nullptr, nullptr, false, initial_vf.data());
		const auto legacy_flags = current_flags();
		Check(EmuCoreXOracleVUIRFtoiInstructions(0) == before_legacy,
			"EE macro FTOI forced legacy does not increment shared counter");
		EmuCoreXOracleSetVUIR(0, 1);
		const u64 before_shared = EmuCoreXOracleVUIRFtoiInstructions(0);
		const EESnapshot shared = RunEEProgramWithIrMode(0, code, initial_gpr.data(), nullptr,
			nullptr, nullptr, false, initial_vf.data());
		const auto shared_flags = current_flags();
		Check(EmuCoreXOracleVUIRFtoiInstructions(0) == before_shared + routed,
			"EE macro FTOI shared route counter matches eligible destination/mask");
		const EESnapshot interp = RunEEProgram(false, code, false, nullptr, nullptr,
			initial_gpr.data(), nullptr, nullptr, initial_vf.data());
		const auto interp_flags = current_flags();
		CompareEE(interp, legacy, "EE COP2 macro FTOI interpreter/legacy parity");
		CompareEE(legacy, shared, "EE COP2 macro FTOI legacy/shared parity");
		bool exact = true;
		for (u32 i = 0; i < expected_vf.size(); ++i)
			exact &= interp.vu0_vf[i] == expected_vf[i] && legacy.vu0_vf[i] == expected_vf[i] &&
				shared.vu0_vf[i] == expected_vf[i];
		for (u32 reg = 0; reg < 32; ++reg)
		{
			const u64 low = reg == 18 ? output_low : initial_gpr[reg].UD[0];
			const u64 high = reg == 18 ? output_high : initial_gpr[reg].UD[1];
			exact &= interp.gpr[reg] == low && legacy.gpr[reg] == low && shared.gpr[reg] == low;
			exact &= interp.gpr_upper[reg] == high && legacy.gpr_upper[reg] == high && shared.gpr_upper[reg] == high;
		}
		Check(exact, label);
		Check(legacy.cycle == shared.cycle, "EE macro FTOI preserves exact legacy/shared cycles");
		Check(interp_flags == legacy_flags && legacy_flags == shared_flags,
			"EE macro FTOI preserves the legacy MAC/STATUS/CLIP flag contract");
	};

	// Every FTOI scale and nonempty mask, with full-register checks around the
	// COP2 transfers. Raw values cover signed fractions and saturation edges.
	for (u32 fraction_index = 0; fraction_index < fractions.size(); ++fraction_index)
	for (u32 mask = 1; mask < 16; ++mask)
	{
		std::array<u32, 128> expected_vf = initial_vf;
		std::array<u32, 4> expected_dest = initial_dest;
		for (u32 lane = 0; lane < 4; ++lane)
			expected_vf[7 * 4 + lane] = source[lane]; // QMTC2 seeds Fs before FTOI.
		for (u32 lane = 0; lane < 4; ++lane)
			if (mask & (8u >> lane))
				expected_vf[9 * 4 + lane] = expected_dest[lane] = VuFtoiExpectedBits(
					source[lane], fractions[fraction_index], static_cast<u32>(EmuConfig.Cpu.VU0FPCR.bitmask));
		const std::vector<u32> code = {qmtc2(8, 7), qmtc2(10, 9), ftoi(fraction_index, mask, 7, 9), qmfc2(18, 9)};
		run_and_check(code, expected_vf, expected_dest, 1,
			"EE macro FTOI matches raw independent model, VF mask, and GPR transfers");
	}

	// Fs==Ft confirms the source is read before the masked destination write.
	for (u32 fraction_index = 0; fraction_index < fractions.size(); ++fraction_index)
	for (u32 mask : {15u, 4u})
	{
		std::array<u32, 128> expected_vf = initial_vf;
		std::array<u32, 4> expected_dest = source;
		for (u32 lane = 0; lane < 4; ++lane)
			expected_vf[7 * 4 + lane] = source[lane]; // QMTC2 seeds the aliased source.
		for (u32 lane = 0; lane < 4; ++lane)
			if (mask & (8u >> lane))
				expected_vf[7 * 4 + lane] = expected_dest[lane] = VuFtoiExpectedBits(
					source[lane], fractions[fraction_index], static_cast<u32>(EmuConfig.Cpu.VU0FPCR.bitmask));
		const std::vector<u32> code = {qmtc2(8, 7), ftoi(fraction_index, mask, 7, 7), qmfc2(18, 7)};
		run_and_check(code, expected_vf, expected_dest, 1,
			"EE macro FTOI alias preserves source lanes and masked neighbors");
	}

	// VF0 remains readable as (0,0,0,1), while VF0 writes and mask zero must
	// stay out of the shared FTOI route entirely.
	for (u32 fraction_index = 0; fraction_index < fractions.size(); ++fraction_index)
	for (const auto boundary : {std::array<u32, 4>{0, 9, 15, 1},
		std::array<u32, 4>{7, 0, 15, 0}, std::array<u32, 4>{7, 9, 0, 0}})
	{
		const u32 fs = boundary[0], ft = boundary[1], mask = boundary[2], routed = boundary[3];
		std::array<u32, 128> expected_vf = initial_vf;
		std::array<u32, 4> expected_dest = ft == 0 ?
			std::array<u32, 4>{0, 0, 0, 0x3f800000u} : initial_dest;
		if (routed)
			for (u32 lane = 0; lane < 4; ++lane)
				if (mask & (8u >> lane))
					expected_vf[ft * 4 + lane] = expected_dest[lane] = VuFtoiExpectedBits(
						initial_vf[fs * 4 + lane], fractions[fraction_index],
						static_cast<u32>(EmuConfig.Cpu.VU0FPCR.bitmask));
		const std::vector<u32> code = {qmtc2(10, 9), ftoi(fraction_index, mask, fs, ft), qmfc2(18, ft)};
		run_and_check(code, expected_vf, expected_dest, routed,
			"EE macro FTOI VF0/mask boundary matches independent raw-bit model");
	}

	EmuCoreXOracleSetVUIR(0, -1);
	EmuCoreXOracleSetEeIrMode(-1);
	EmuConfig.Cpu.FPUFPCR = original_ee_fpcr;
	EmuConfig.Cpu.VU0FPCR = original_vu0_fpcr;
}

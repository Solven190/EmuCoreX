// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+
//
// EE COP2 macro ITOF integration tests. Include after the EE run helpers and
// the shared VU ITOF oracle model are declared.

void EEVuItofMacroTests()
{
	const auto original_ee_fpcr = EmuConfig.Cpu.FPUFPCR;
	const auto original_vu0_fpcr = EmuConfig.Cpu.VU0FPCR;
	EmuConfig.Cpu.FPUFPCR.SetRoundMode(FPRoundMode::Nearest);
	EmuConfig.Cpu.VU0FPCR.SetRoundMode(FPRoundMode::Nearest);
	const FPControlRegisterBackup host_fpcr(EmuConfig.Cpu.FPUFPCR);
	const auto qmtc2 = [](u32 rt, u32 vd) {
		return (0x12u << 26) | (5u << 21) | (rt << 16) | (vd << 11);
	};
	const auto qmfc2 = [](u32 rt, u32 vd) {
		return (0x12u << 26) | (1u << 21) | (rt << 16) | (vd << 11);
	};
	const auto itof = [](u32 fraction_index, u32 mask, u32 fs, u32 ft) {
		constexpr std::array<u32, 4> major = {0x3c, 0x3d, 0x3e, 0x3f};
		return (0x12u << 26) | ((0x10u | mask) << 21) | (ft << 16) | (fs << 11) |
			(4u << 6) | major[fraction_index];
	};
	constexpr std::array<u32, 4> fractions = {0, 4, 12, 15};
	constexpr std::array<u32, 4> source = {0x01000001u, 0xfeffffffu, 0x80000000u, 0x7fffffffu};
	constexpr std::array<u32, 4> initial_dest = {0xdeadbeefu, 0xcafebabeu, 0x81234567u, 0x7fc54321u};
	std::array<u32, 128> initial_vf{};
	initial_vf[3] = 0x3f800000u;
	std::array<GPR_reg, 32> initial_gpr{};
	initial_gpr[8].UD[0] = static_cast<u64>(source[0]) | (static_cast<u64>(source[1]) << 32);
	initial_gpr[8].UD[1] = static_cast<u64>(source[2]) | (static_cast<u64>(source[3]) << 32);
	initial_gpr[10].UD[0] = static_cast<u64>(initial_dest[0]) | (static_cast<u64>(initial_dest[1]) << 32);
	initial_gpr[10].UD[1] = static_cast<u64>(initial_dest[2]) | (static_cast<u64>(initial_dest[3]) << 32);
	const auto flags = [] { return std::array<u32, 3>{VU0.macflag, VU0.statusflag, VU0.clipflag}; };

	const auto check_gprs = [&](const EESnapshot& interp, const EESnapshot& legacy,
		const EESnapshot& shared, u64 output_low, u64 output_high, const char* name) {
		bool same = true;
		for (u32 reg = 0; reg < 32; ++reg)
		{
			const u64 low = reg == 18 ? output_low : initial_gpr[reg].UD[0];
			const u64 high = reg == 18 ? output_high : initial_gpr[reg].UD[1];
			same &= interp.gpr[reg] == low && legacy.gpr[reg] == low && shared.gpr[reg] == low;
			same &= interp.gpr_upper[reg] == high && legacy.gpr_upper[reg] == high && shared.gpr_upper[reg] == high;
		}
		Check(same, name);
	};

	for (u32 fraction_index = 0; fraction_index < fractions.size(); ++fraction_index)
	for (u32 mask = 1; mask < 16; ++mask)
	{
		const std::vector<u32> code = {qmtc2(8, 7), qmtc2(10, 9),
			itof(fraction_index, mask, 7, 9), qmfc2(18, 9)};
		EmuCoreXOracleSetEeIrMode(0);
		EmuCoreXOracleSetVUIR(0, 0);
		const u64 before_legacy = EmuCoreXOracleVUIRItofInstructions(0);
		const EESnapshot legacy = RunEEProgramWithIrMode(0, code, initial_gpr.data(), nullptr,
			nullptr, nullptr, false, initial_vf.data());
		Check(EmuCoreXOracleVUIRItofInstructions(0) == before_legacy,
			"EE macro ITOF legacy path does not increment shared counter");
		const auto legacy_flags = flags();
		const u64 before_shared = EmuCoreXOracleVUIRItofInstructions(0);
		EmuCoreXOracleSetVUIR(0, 1);
		const EESnapshot shared = RunEEProgramWithIrMode(0, code, initial_gpr.data(), nullptr,
			nullptr, nullptr, false, initial_vf.data());
		const auto shared_flags = flags();
		Check(EmuCoreXOracleVUIRItofInstructions(0) == before_shared + 1,
			"EE macro ITOF routes through shared VU IR exactly once");
		const EESnapshot interp = RunEEProgram(false, code, false, nullptr, nullptr,
			initial_gpr.data(), nullptr, nullptr, initial_vf.data());
		const auto interp_flags = flags();
		CompareEE(interp, legacy, "EE COP2 macro ITOF interpreter/legacy parity");
		CompareEE(legacy, shared, "EE COP2 macro ITOF legacy/shared parity");

		std::array<u32, 4> expected_lanes = initial_dest;
		for (u32 lane = 0; lane < 4; ++lane)
			if (mask & (8u >> lane))
				expected_lanes[lane] = VuItofExpectedBits(source[lane], fractions[fraction_index],
					VuItofRounding::NearestEven);
		bool expected = true;
		for (u32 lane = 0; lane < 4; ++lane)
			expected &= interp.vu0_vf[9 * 4 + lane] == expected_lanes[lane] &&
				legacy.vu0_vf[9 * 4 + lane] == expected_lanes[lane] &&
				shared.vu0_vf[9 * 4 + lane] == expected_lanes[lane];
		const u64 output_low = static_cast<u64>(expected_lanes[0]) | (static_cast<u64>(expected_lanes[1]) << 32);
		const u64 output_high = static_cast<u64>(expected_lanes[2]) | (static_cast<u64>(expected_lanes[3]) << 32);
		expected &= interp.gpr[18] == output_low && legacy.gpr[18] == output_low && shared.gpr[18] == output_low &&
			interp.gpr_upper[18] == output_high && legacy.gpr_upper[18] == output_high && shared.gpr_upper[18] == output_high;
		Check(expected, "EE macro ITOF matches independent signed-integer bit model and QMFC2 result");
		check_gprs(interp, legacy, shared, output_low, output_high,
			"EE macro ITOF preserves all GPRs except QMFC2 destination");
		Check(legacy.cycle == shared.cycle, "EE macro ITOF preserves exact legacy/shared cycles");
		Check(interp_flags == legacy_flags && legacy_flags == shared_flags &&
			shared_flags == std::array<u32, 3>{0, 0, 0},
			"EE macro ITOF preserves zero MAC/STATUS/CLIP state");
	}

	// Macro aliasing confirms the source is bound before the writable destination.
	for (u32 mask : {15u, 4u})
	{
		const std::vector<u32> code = {qmtc2(8, 7), itof(2, mask, 7, 7), qmfc2(18, 7)};
		EmuCoreXOracleSetVUIR(0, 0);
		const EESnapshot legacy = RunEEProgramWithIrMode(0, code, initial_gpr.data(), nullptr,
			nullptr, nullptr, false, initial_vf.data());
		const u64 before = EmuCoreXOracleVUIRItofInstructions(0);
		EmuCoreXOracleSetVUIR(0, 1);
		const EESnapshot shared = RunEEProgramWithIrMode(0, code, initial_gpr.data(), nullptr,
			nullptr, nullptr, false, initial_vf.data());
		const EESnapshot interp = RunEEProgram(false, code, false, nullptr, nullptr,
			initial_gpr.data(), nullptr, nullptr, initial_vf.data());
		CompareEE(interp, legacy, "EE macro ITOF alias interpreter/legacy parity");
		CompareEE(legacy, shared, "EE macro ITOF alias legacy/shared parity");
		std::array<u32, 4> expected = source;
		for (u32 lane = 0; lane < 4; ++lane)
			if (mask & (8u >> lane))
				expected[lane] = VuItofExpectedBits(source[lane], 12, VuItofRounding::NearestEven);
		bool exact = true;
		for (u32 lane = 0; lane < 4; ++lane)
			exact &= interp.vu0_vf[7 * 4 + lane] == expected[lane] &&
				legacy.vu0_vf[7 * 4 + lane] == expected[lane] && shared.vu0_vf[7 * 4 + lane] == expected[lane];
		Check(exact && legacy.cycle == shared.cycle &&
			EmuCoreXOracleVUIRItofInstructions(0) == before + 1,
			"EE macro ITOF alias preserves lanes, cycles, and routing counter");
	}

	// VF0 is immutable but readable; VF0 destination and mask zero remain unrouted.
	for (const auto boundary : {std::array<u32, 4>{0, 9, 15, 1},
		std::array<u32, 4>{7, 0, 15, 0}, std::array<u32, 4>{7, 9, 0, 0}})
	{
		const u32 fs = boundary[0], ft = boundary[1], mask = boundary[2], routed = boundary[3];
		const std::vector<u32> code = {qmtc2(10, 9), itof(1, mask, fs, ft), qmfc2(18, ft)};
		EmuCoreXOracleSetVUIR(0, 0);
		const EESnapshot legacy = RunEEProgramWithIrMode(0, code, initial_gpr.data(), nullptr,
			nullptr, nullptr, false, initial_vf.data());
		EmuCoreXOracleSetVUIR(0, 1);
		const u64 before = EmuCoreXOracleVUIRItofInstructions(0);
		const EESnapshot shared = RunEEProgramWithIrMode(0, code, initial_gpr.data(), nullptr,
			nullptr, nullptr, false, initial_vf.data());
		const EESnapshot interp = RunEEProgram(false, code, false, nullptr, nullptr,
			initial_gpr.data(), nullptr, nullptr, initial_vf.data());
		CompareEE(interp, legacy, "EE ITOF VF0/mask boundary interpreter/legacy parity");
		CompareEE(legacy, shared, "EE ITOF VF0/mask boundary legacy/shared parity");
		Check(legacy.cycle == shared.cycle && EmuCoreXOracleVUIRItofInstructions(0) == before + routed,
			"EE ITOF VF0/mask boundary preserves cycles and routing admission");
		std::array<u32, 4> expected_lanes = ft == 0 ?
			std::array<u32, 4>{0, 0, 0, 0x3f800000u} : initial_dest;
		for (u32 lane = 0; lane < 4; ++lane)
			if (routed && (mask & (8u >> lane)))
				expected_lanes[lane] = VuItofExpectedBits(initial_vf[fs * 4 + lane], 4,
					VuItofRounding::NearestEven);
		bool independent = true;
		for (u32 lane = 0; lane < 4; ++lane)
			independent &= interp.vu0_vf[ft * 4 + lane] == expected_lanes[lane] &&
				legacy.vu0_vf[ft * 4 + lane] == expected_lanes[lane] &&
				shared.vu0_vf[ft * 4 + lane] == expected_lanes[lane];
		Check(independent, "EE ITOF VF0/mask boundary matches independent raw-bit expectation");
	}
	EmuCoreXOracleSetVUIR(0, -1);
	EmuCoreXOracleSetEeIrMode(-1);
	EmuConfig.Cpu.FPUFPCR = original_ee_fpcr;
	EmuConfig.Cpu.VU0FPCR = original_vu0_fpcr;
}

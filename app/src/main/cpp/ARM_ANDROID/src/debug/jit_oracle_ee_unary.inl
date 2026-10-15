// SPDX-License-Identifier: GPL-3.0+
// COP1 ABS.S/NEG.S raw-bit integration tests. This fragment relies on
// the existing RunEEProgram, RunEEProgramWithIrMode, RunEEIrCop1Direct,
// MipsCop1, CompareEE, and Check helpers in jit_oracle.cpp.
extern "C" u64 EmuCoreXOracleGetEeIrUnaryCounts();
void EEIrCop1UnaryTests()
{
	constexpr std::array<u32, 8> raw_bits = {
		0x00000000u, 0x80000000u, 0x00000001u, 0x80000001u,
		0x7f800000u, 0xff800000u, 0x7fc12345u, 0xff812345u};
	constexpr std::array<u32, 2> functions = {0x05, 0x07}; // ABS.S, NEG.S
	constexpr std::array<u32, 2> initial_fcr31 = {0xa5a5c058u, 0x5a5a0058u};
	std::array<GPR_reg, 32> initial_gpr{};
	std::array<u32, 32> initial_fpr{}, initial_fcr{};
	std::array<u32, 32> initial_cp0{};
	initial_cp0[12] = 0x20000000u; // Status.CU1
	for (u32 reg = 0; reg < 32; ++reg)
	{
		if (reg != 0)
		{
			initial_gpr[reg].UD[0] = 0x1234567800000000ull | (0x76543210u + reg);
			initial_gpr[reg].UD[1] = 0xabcdef0100000000ull | (0x89abcdefu + reg);
		}
		initial_fpr[reg] = 0x12345678u ^ (reg * 0x01020408u);
		initial_fcr[reg] = 0x87654321u ^ (reg * 0x01020304u);
	}
	initial_fcr[0] = 0x2e30u;

	bool direct_lifted_all = true;
	for (u32 funct : functions)
	for (u32 pattern = 0; pattern < raw_bits.size(); ++pattern)
	for (u32 fs = 0; fs < 32; ++fs)
	for (u32 destination_case = 0; destination_case < 2; ++destination_case)
	for (u32 fcr_case = 0; fcr_case < initial_fcr31.size(); ++fcr_case)
	{
		const u32 fd = destination_case == 0 ? fs : ((fs + 1) & 31);
		auto fpr = initial_fpr;
		auto fcr = initial_fcr;
		fpr[fs] = raw_bits[pattern];
		fcr[31] = initial_fcr31[fcr_case];
		const std::vector<u32> code = {MipsCop1(16, 0, fs, fd, funct)};

		const EESnapshot interp = RunEEProgram(false, code, false, nullptr, initial_cp0.data(),
			initial_gpr.data(), fpr.data(), fcr.data());
		const EESnapshot legacy = RunEEProgramWithIrMode(0, code, initial_gpr.data(), initial_cp0.data(),
			fpr.data(), fcr.data());
		const u64 compiled_before = EmuCoreXOracleGetEeIrUnaryCounts();
		const EESnapshot shared = RunEEProgramWithIrMode(1, code, initial_gpr.data(), initial_cp0.data(),
			fpr.data(), fcr.data());
		Check(EmuCoreXOracleGetEeIrUnaryCounts() == compiled_before + 1,
			"COP1 ABS.S/NEG.S production compilation uses shared IR");
		EESnapshot direct{};
		const bool direct_ok = RunEEIrCop1Direct(code, initial_gpr.data(), fpr.data(), fcr.data(), direct);
		direct_lifted_all &= direct_ok;
		CompareEE(interp, legacy, "COP1 unary interpreter/legacy raw state");
		CompareEE(interp, shared, "COP1 unary interpreter/shared production state");
		if (direct_ok)
			CompareEE(interp, direct, "COP1 unary interpreter/direct IR full state");
		Check(legacy.cycle == shared.cycle,
			"COP1 ABS.S/NEG.S legacy/shared exact cycles match");

		auto expected_fpr = fpr;
		auto expected_fcr = fcr;
		expected_fpr[fd] = funct == 0x05 ? (raw_bits[pattern] & 0x7fffffffu) :
			(raw_bits[pattern] ^ 0x80000000u);
		// ABS.S and NEG.S clear only the current O/U cause bits. Sticky SO/SU,
		// condition, rounding, and every unrelated FCR word remain untouched.
		expected_fcr[31] &= ~0x0000c000u;
		const bool independent = std::memcmp(interp.fpr, expected_fpr.data(), sizeof(interp.fpr)) == 0 &&
			std::memcmp(legacy.fpr, expected_fpr.data(), sizeof(legacy.fpr)) == 0 &&
			std::memcmp(shared.fpr, expected_fpr.data(), sizeof(shared.fpr)) == 0 &&
			std::memcmp(interp.fcr, expected_fcr.data(), sizeof(interp.fcr)) == 0 &&
			std::memcmp(legacy.fcr, expected_fcr.data(), sizeof(legacy.fcr)) == 0 &&
			std::memcmp(shared.fcr, expected_fcr.data(), sizeof(shared.fcr)) == 0;
		Check(independent, "COP1 ABS.S/NEG.S raw sign transform and FCR31 cause/sticky bits match independent model");
		bool gpr_exact = true;
		for (u32 reg = 0; reg < 32; ++reg)
		{
			const u64 expected_low = reg == 0 ? 0 : initial_gpr[reg].UD[0];
			const u64 expected_high = reg == 0 ? 0 : initial_gpr[reg].UD[1];
			gpr_exact &= interp.gpr[reg] == expected_low && legacy.gpr[reg] == expected_low &&
				shared.gpr[reg] == expected_low;
			gpr_exact &= interp.gpr_upper[reg] == expected_high && legacy.gpr_upper[reg] == expected_high &&
				shared.gpr_upper[reg] == expected_high;
		}
		Check(gpr_exact, "COP1 ABS.S/NEG.S preserves all GPRs and keeps architectural GPR0 zero");
		Check(direct_ok, "COP1 ABS.S/NEG.S frontend admits a raw FPR/FCR IR slice");
	}
	Check(direct_lifted_all, "COP1 ABS.S/NEG.S frontend accepts every source/destination alias pattern");
	for (u32 funct : functions)
	for (u32 branch_case = 0; branch_case < 3; ++branch_case)
	{
		auto gpr = initial_gpr;
		auto fpr = initial_fpr;
		auto fcr = initial_fcr;
		gpr[8].UD[0] = 1;
		fpr[5] = 0xff812345u;
		fpr[6] = 0x12345678u;
		fcr[31] = 0xa5a5c058u;
		const bool annulled = branch_case == 2;
		const u32 branch = branch_case == 0 ? MipsI(4, 0, 0, 1) :
			MipsI(annulled ? 0x14 : 4, 8, 0, 1);
		const std::vector<u32> code = {branch, MipsCop1(16, 0, 5, 6, funct)};
		const EESnapshot interp = RunEEProgram(false, code, false, nullptr,
			initial_cp0.data(), gpr.data(), fpr.data(), fcr.data());
		const u64 compiled_before = EmuCoreXOracleGetEeIrUnaryCounts();
		const EESnapshot legacy = RunEEProgramWithIrMode(0, code, gpr.data(),
			initial_cp0.data(), fpr.data(), fcr.data());
		Check(EmuCoreXOracleGetEeIrUnaryCounts() == compiled_before,
			"COP1 unary branch legacy mode does not compile shared IR");
		const EESnapshot shared = RunEEProgramWithIrMode(1, code, gpr.data(),
			initial_cp0.data(), fpr.data(), fcr.data());
		Check(EmuCoreXOracleGetEeIrUnaryCounts() == compiled_before + 1,
			"COP1 unary branch/delay/annul block compiles through shared IR");
		CompareEE(interp, legacy, "COP1 unary branch delay interpreter/legacy");
		CompareEE(interp, shared, "COP1 unary branch delay interpreter/shared");
		const u32 result = funct == 0x05 ? (fpr[5] & 0x7fffffffu) : (fpr[5] ^ 0x80000000u);
		Check(shared.fpr[6] == (annulled ? fpr[6] : result) &&
			shared.fcr[31] == (annulled ? fcr[31] : fcr[31] & ~0xc000u),
			"COP1 unary delay/annul independently controls raw result and cause clearing");
		Check(legacy.cycle == shared.cycle, "COP1 unary delay/annul exact JIT cycles");
	}
}

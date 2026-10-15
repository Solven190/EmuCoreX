// SPDX-License-Identifier: GPL-3.0+
// EE load-hotspot probe for the GTA 0x0034dd58 block.
// Include after RunEEProgram/RunEEProgramWithIrMode and EE snapshots are declared.

extern "C" bool EmuCoreXOracleGetEeCompiledBlock(u32 pc, const void** host, u32* bytes);
extern "C" u64 EmuCoreXOracleGetEeIrDirectLoadCounts();

namespace
{
u32 s_ee_hot_load_compile_pc = 0;
u64 s_ee_hot_load_compile_count = 0;

void EeHotLoadCompileCallback(u32 pc)
{
	if (pc == s_ee_hot_load_compile_pc)
		++s_ee_hot_load_compile_count;
}

std::array<GPR_reg, 32> EeHotLoadInitialGprs(u32 gp)
{
	std::array<GPR_reg, 32> regs{};
	for (u32 r = 1; r < regs.size(); ++r)
	{
		regs[r].UD[0] = 0x1234567800000000ull | (0x10203040u + r * 0x101u);
		regs[r].UD[1] = 0xabcdef0100000000ull | (0x50607080u + r * 0x103u);
	}
	regs[0] = {};
	regs[28].UD[0] = gp;
	return regs;
}

std::array<GPR_reg, 32> EeHotLoadExpectedGprs(const std::array<GPR_reg, 32>& initial,
	int workload, u32 value_v1, u32 value_v0, u32 value_s0)
{
	std::array<GPR_reg, 32> expected = initial;
	if (workload == 0)
	{
		expected[3].UD[0] = static_cast<u64>(static_cast<s64>(static_cast<s32>(value_v1)));
		expected[2].UD[0] = static_cast<u64>(static_cast<s64>(static_cast<s32>(value_v0)));
		expected[16].UD[0] = value_s0;
	}
	else if (workload == 1)
		expected[2].UD[0] = static_cast<u64>(static_cast<s64>(static_cast<s32>(value_v0)));
	else if (workload == 2)
		expected[4].UD[0] = initial[16].UD[0];
	return expected;
}

bool EeHotLoadCheckGprs(const EESnapshot& result, const std::array<GPR_reg, 32>& expected)
{
	bool same = true;
	for (u32 r = 0; r < 32; ++r)
		same &= result.gpr[r] == expected[r].UD[0] && result.gpr_upper[r] == expected[r].UD[1];
	return same;
}

EESnapshot EeHotLoadRunInterpreterFirstBlock(const std::vector<u32>& program,
	const std::array<GPR_reg, 32>& initial_gpr, const std::array<u32, 32>& initial_cp0, u32 steps = 4)
{
	std::memset(&cpuRegs, 0, sizeof(cpuRegs));
	std::memset(&fpuRegs, 0, sizeof(fpuRegs));
	std::memset(&VU0.VF, 0, sizeof(VU0.VF));
	std::memset(&VU0.VI, 0, sizeof(VU0.VI));
	std::memset(&VU0.ACC, 0, sizeof(VU0.ACC));
	VU0.q.UL = VU0.p.UL = 0;
	VU0.macflag = VU0.statusflag = VU0.clipflag = 0;
	std::vector<u32> code = program;
	code.push_back(0x1000ffffu);
	code.push_back(0);
	for (u32 i = 0; i < code.size(); ++i)
		memWrite32(EE_TEST_PC + i * 4, code[i]);
	for (u32 i = 0; i < EE_SCRATCH_SIZE; ++i)
		memWrite8(EE_TEST_SCRATCH + i, static_cast<u8>(i * 13 + 7));
	std::memcpy(cpuRegs.GPR.r, initial_gpr.data(), sizeof(cpuRegs.GPR.r));
	std::memcpy(cpuRegs.CP0.r, initial_cp0.data(), sizeof(cpuRegs.CP0.r));
	cpuRegs.pc = EE_TEST_PC;
	cpuRegs.cycle = 0;
	cpuRegs.branch = 0;
	cpuRegs.nextEventCycle = 0x7fffffffu;
	EEsCycle = EEoCycle = 0;
	Cpu = &intCpu;
	intCpu.Reset();
	// EE branch execution is atomic with its delay instruction in the
	// interpreter, so three ordinary instructions plus the branch cover the
	// exact first compiled block.
	EmuCoreXOracleEESteps(steps);
	EESnapshot out;
	CaptureEE(out);
	return out;
}

bool EeHotLoadCaptureBlock(std::vector<u8>* code, u32* profile_bytes, u64* host_base)
{
	if (!code || !profile_bytes || !host_base)
		return false;
	const void* host = nullptr;
	u32 bytes = 0;
	const bool found = EmuCoreXOracleGetEeCompiledBlock(EE_TEST_PC, &host, &bytes) && host && bytes;
	std::vector<JitBlockProfile> profiles;
	EE_JitGetBlockProfiles(profiles);
	u32 matches = 0;
	*profile_bytes = 0;
	for (const JitBlockProfile& profile : profiles)
	{
		if (profile.startpc == EE_TEST_PC)
		{
			++matches;
			*profile_bytes = profile.host_size;
		}
	}
	if (!found || matches != 1 || *profile_bytes != bytes)
		return false;
	code->assign(static_cast<const u8*>(host), static_cast<const u8*>(host) + bytes);
	*host_base = reinterpret_cast<u64>(host);
	return true;
}

bool EeHotLoadWriteDump(const char* workload, const char* values, const char* mode,
	const char* phase, const std::vector<u8>& code, u64 host_base)
{
	char path[192];
	std::snprintf(path, sizeof(path), "artifacts/ee-hot-%s-%s-%s-%s.bin", workload, values, mode, phase);
	std::FILE* file = std::fopen(path, "wb");
	if (!file)
		return false;
	const size_t written = std::fwrite(code.data(), 1, code.size(), file);
	const int close_result = std::fclose(file);
	if (written == code.size() && close_result == 0)
		std::printf("EEHOTLOAD_DUMP guest_pc=%08x host_base=0x%llx path=%s bytes=%zu\n",
			EE_TEST_PC, static_cast<unsigned long long>(host_base), path, code.size());
	return written == code.size() && close_result == 0;
}

void EeHotLoadResetJitState(const std::array<GPR_reg, 32>& initial_gpr,
	const std::array<u32, 32>& initial_cp0)
{
	std::memcpy(cpuRegs.GPR.r, initial_gpr.data(), sizeof(cpuRegs.GPR.r));
	std::memcpy(cpuRegs.CP0.r, initial_cp0.data(), sizeof(cpuRegs.CP0.r));
	cpuRegs.CP0.n.Status.val = 0x20000000u;
	cpuRegs.pc = EE_TEST_PC;
	cpuRegs.cycle = 0;
	cpuRegs.branch = 0;
	cpuRegs.nextEventCycle = 0x7fffffffu;
	EEsCycle = EEoCycle = 0;
	EmuCoreXEEForceExitAfterFirstBlock();
	Cpu->Execute();
}

}

void EEHotLoadBenchmarkTests()
{
	// The first five words are the captured GTA sequence verbatim. Four NOPs
	// occupy the original branch target at PC+0x24, so both paths reach the
	// helper's self-branch without changing the five-word hotspot itself.
	constexpr std::array<u32, 5> gta_hot_words = {
		0x8f8310f8u, // lw v1, 0x10f8(gp)
		0x0000802du, // daddu s0, zero, zero
		0x8f8210fcu, // lw v0, 0x10fc(gp)
		0x54620005u, // bnel v1, v0, +5
		0x24100001u, // addiu s0, zero, 1 (likely delay slot)
	};
	constexpr u32 data_address = EE_TEST_SCRATCH + 0x1000;
	constexpr u32 one_lw_address = EE_TEST_SCRATCH + 0x1100;
	const std::vector<u32> gta_hot_program = {gta_hot_words[0], gta_hot_words[1], gta_hot_words[2],
		gta_hot_words[3], gta_hot_words[4], 0, 0, 0, 0};
	const std::array<u32, 32> initial_cp0 = [] {
		std::array<u32, 32> cp0{};
		cp0[12] = 0x20000000u;
		return cp0;
	}();
	constexpr u32 equal_value = 0xf1234567u;
	constexpr u32 unequal_value = 0x01234567u;
	const std::array<std::array<u32, 2>, 2> values = {{{equal_value, equal_value},
		{equal_value, unequal_value}}};
	std::array<EESnapshot, 2> expected_jit{};

	for (u32 value_case = 0; value_case < values.size(); ++value_case)
	{
		const char* value_name = value_case ? "unequal" : "equal";
		const u32 first = values[value_case][0];
		const u32 second = values[value_case][1];
		memWrite32(data_address, first);
		memWrite32(data_address + 4, second);
		const auto initial_gpr = EeHotLoadInitialGprs(data_address - 0x10f8u);

		const EESnapshot interp = EeHotLoadRunInterpreterFirstBlock(gta_hot_program,
			initial_gpr, initial_cp0);
		for (int mode : {0, 1})
		{
			EmuCoreXOracleSetEeIrMode(mode);
			const char* mode_name = mode ? "shared" : "legacy";
			const u64 direct_loads_before = EmuCoreXOracleGetEeIrDirectLoadCounts();
			const EESnapshot result = RunEEProgramWithIrMode(mode, gta_hot_program,
				initial_gpr.data(), initial_cp0.data(), nullptr, nullptr, false);
			const u64 direct_loads = EmuCoreXOracleGetEeIrDirectLoadCounts() - direct_loads_before;
			expected_jit[mode] = result;
			CompareEE(interp, result, mode ? "GTA five-word load hotspot interpreter/shared" :
				"GTA five-word load hotspot interpreter/legacy");
			const auto expected = EeHotLoadExpectedGprs(initial_gpr, 0, first, second,
				value_case ? 1u : 0u);
			Check(EeHotLoadCheckGprs(result, expected) && EeHotLoadCheckGprs(interp, expected),
				"GTA load hotspot interpreter/JIT match independent full-GPR expectation");
			Check(direct_loads == (mode ? 2u : 0u),
				"GTA load hotspot cold route admits exactly two direct loads only in shared mode");
			std::vector<u8> code;
			u32 profile_bytes = 0;
			u64 host_base = 0;
			const bool captured = EeHotLoadCaptureBlock(&code, &profile_bytes, &host_base);
			Check(captured && profile_bytes > 0, "GTA load hotspot cold block has native code and profile bytes");
			Check(!captured || EeHotLoadWriteDump("gta-34dd58", value_name, mode_name, "cold", code, host_base),
				"GTA load hotspot cold native block dump is written in runner artifact directory");
		}
		EmuCoreXOracleSetEeIrMode(-1);
		const EESnapshot legacy = RunEEProgramWithIrMode(0, gta_hot_program,
			initial_gpr.data(), initial_cp0.data(), nullptr, nullptr, false);
		const EESnapshot shared = RunEEProgramWithIrMode(1, gta_hot_program,
			initial_gpr.data(), initial_cp0.data(), nullptr, nullptr, false);
		CompareEE(legacy, shared, "GTA five-word load hotspot legacy/shared full state");
		Check(legacy.cycle == shared.cycle, "GTA five-word load hotspot exact legacy/shared JIT cycles");

		for (int mode : {0, 1})
		{
			const char* mode_name = mode ? "shared" : "legacy";
			EmuCoreXOracleSetEeIrMode(mode);
			const u32 gp = data_address - 0x10f8u;
			const auto bench_gpr = EeHotLoadInitialGprs(gp);
			s_ee_hot_load_compile_pc = EE_TEST_PC;
			s_ee_hot_load_compile_count = 0;
			EmuCoreXEEOracleSetCompileCallback(EeHotLoadCompileCallback);
			std::array<double, 7> cold_us{};
			std::vector<u8> cold_code;
			u32 cold_bytes = 0;
			u64 cold_host_base = 0;
			bool exact_cold_compile = true;
			for (u32 sample = 0; sample < cold_us.size(); ++sample)
			{
				const u64 before = s_ee_hot_load_compile_count;
				const u64 direct_loads_before = EmuCoreXOracleGetEeIrDirectLoadCounts();
				const auto start = std::chrono::steady_clock::now();
				const EESnapshot cold = RunEEProgramWithIrMode(mode, gta_hot_program,
					bench_gpr.data(), initial_cp0.data(), nullptr, nullptr, false);
				cold_us[sample] = std::chrono::duration<double, std::micro>(
					std::chrono::steady_clock::now() - start).count();
				exact_cold_compile &= s_ee_hot_load_compile_count - before == 1;
				const u64 direct_loads = EmuCoreXOracleGetEeIrDirectLoadCounts() - direct_loads_before;
				exact_cold_compile &= direct_loads == (mode ? 2u : 0u);
				Check(EeHotLoadCheckGprs(cold, EeHotLoadExpectedGprs(bench_gpr, 0,
					first, second, value_case ? 1u : 0u)),
					"GTA load hotspot cold state matches independent GPR expectation");
				if (!EeHotLoadCaptureBlock(&cold_code, &cold_bytes, &cold_host_base))
					exact_cold_compile = false;
			}
			std::sort(cold_us.begin(), cold_us.end());
			Check(exact_cold_compile, "GTA hotspot cold samples compile exactly one target EE block");
			Check(cold_bytes > 0 && EeHotLoadWriteDump("gta-34dd58", value_name,
				mode_name, "cold", cold_code, cold_host_base), "GTA hotspot cold native bytes are captured to artifact file");

			// Repeat the first-block execution enough times to pass the EE code-page
			// protection/recompile guard before timing steady state.
			EmuCoreXOracleSetEeIrMode(mode);
			constexpr u32 warm_replays = 65536;
			for (u32 repeat = 0; repeat < warm_replays; ++repeat)
				EeHotLoadResetJitState(bench_gpr, initial_cp0);
			const u64 warm_compile_count = s_ee_hot_load_compile_count;
			const u64 warm_direct_load_count = EmuCoreXOracleGetEeIrDirectLoadCounts();
			std::vector<u8> steady_code;
			u32 steady_bytes = 0;
			u64 steady_host_base = 0;
			const bool steady_captured = EeHotLoadCaptureBlock(&steady_code, &steady_bytes, &steady_host_base);
			Check(steady_captured && steady_bytes > 0,
				"GTA hotspot steady block has native code after guard warmup");
			Check(!steady_captured || EeHotLoadWriteDump("gta-34dd58", value_name,
				mode_name, "steady", steady_code, steady_host_base), "GTA hotspot steady native block dump is written");
			std::array<double, 9> hot_ns{};
			bool stable_hot = true;
			for (u32 sample = 0; sample < hot_ns.size(); ++sample)
			{
				constexpr u32 repeats = 4096;
				const auto start = std::chrono::steady_clock::now();
				for (u32 repeat = 0; repeat < repeats; ++repeat)
					EeHotLoadResetJitState(bench_gpr, initial_cp0);
				hot_ns[sample] = std::chrono::duration<double, std::nano>(
					std::chrono::steady_clock::now() - start).count() / repeats;
				stable_hot &= s_ee_hot_load_compile_count == warm_compile_count &&
					EmuCoreXOracleGetEeIrDirectLoadCounts() == warm_direct_load_count;
				std::vector<u8> probe_code;
				u32 probe_bytes = 0;
				u64 probe_host_base = 0;
				stable_hot &= EeHotLoadCaptureBlock(&probe_code, &probe_bytes, &probe_host_base) &&
					probe_bytes == steady_bytes && probe_code == steady_code;
				EESnapshot hot_state{};
				CaptureEE(hot_state);
				stable_hot &= EeHotLoadCheckGprs(hot_state, EeHotLoadExpectedGprs(bench_gpr, 0,
					first, second, value_case ? 1u : 0u)) && hot_state.cycle == expected_jit[mode].cycle &&
					hot_state.pc == expected_jit[mode].pc;
			}
			std::sort(hot_ns.begin(), hot_ns.end());
			Check(stable_hot, "GTA hotspot hot samples have no recompiles and stable native bytes");
			const u64 hot_compile_count = s_ee_hot_load_compile_count;
			Check(hot_compile_count == warm_compile_count,
				"GTA hotspot compile callback confirms no hot-window recompile");
			std::printf("EEHOTLOAD workload=gta-34dd58 values=%s mode=%s cold_reset_setup_compile_execute_median_us=%.3f hot_reset_dispatch_block_median_ns=%.3f cold_bytes=%u steady_bytes=%u warm_replays=%u hot_samples=9 repeats=4096\n",
				value_name, mode_name, cold_us[cold_us.size() / 2], hot_ns[hot_ns.size() / 2],
				cold_bytes, steady_bytes, warm_replays);
			EmuCoreXEEOracleSetCompileCallback(nullptr);
			EmuCoreXOracleSetEeIrMode(-1);
		}
	}

	// Compact one-LW and branch-only controls use the exact extracted words.
	{
		const u32 value = 0x89abcdefu;
		memWrite32(one_lw_address, value);
		const auto initial = EeHotLoadInitialGprs(one_lw_address + 14376u);
		const std::vector<u32> code = {0x8f82c7d8u}; // lw v0, -14376(gp)
		const EESnapshot interp = RunEEProgram(false, code, false, nullptr, initial_cp0.data(), initial.data());
		const u64 legacy_loads_before = EmuCoreXOracleGetEeIrDirectLoadCounts();
		const EESnapshot legacy = RunEEProgramWithIrMode(0, code, initial.data(), initial_cp0.data(), nullptr, nullptr, false);
		const u64 legacy_loads = EmuCoreXOracleGetEeIrDirectLoadCounts() - legacy_loads_before;
		const u64 shared_loads_before = EmuCoreXOracleGetEeIrDirectLoadCounts();
		const EESnapshot shared = RunEEProgramWithIrMode(1, code, initial.data(), initial_cp0.data(), nullptr, nullptr, false);
		const u64 shared_loads = EmuCoreXOracleGetEeIrDirectLoadCounts() - shared_loads_before;
		CompareEE(interp, legacy, "GTA one-LW control interpreter/legacy");
		CompareEE(legacy, shared, "GTA one-LW control legacy/shared");
		const auto expected = EeHotLoadExpectedGprs(initial, 1, 0, value, 0);
		Check(EeHotLoadCheckGprs(interp, expected) && EeHotLoadCheckGprs(legacy, expected) &&
			EeHotLoadCheckGprs(shared, expected), "GTA one-LW control matches independent full-GPR result");
		Check(legacy_loads == 0 && shared_loads == 1,
			"GTA one-LW control admits exactly one direct load only in shared mode");
		Check(legacy.cycle == shared.cycle, "GTA one-LW control exact legacy/shared cycles");
	}
	{
		const auto initial = EeHotLoadInitialGprs(0);
		const std::vector<u32> code = {0x10400003u, 0x0200202du, 0, 0};
		// An untaken ordinary BEQ leaves the slot to the next interpreter
		// step; taken branches execute their slot atomically.
		const EESnapshot interp = EeHotLoadRunInterpreterFirstBlock(code, initial, initial_cp0, 2);
		const u64 legacy_loads_before = EmuCoreXOracleGetEeIrDirectLoadCounts();
		const EESnapshot legacy = RunEEProgramWithIrMode(0, code, initial.data(), initial_cp0.data(), nullptr, nullptr, false);
		const u64 legacy_loads = EmuCoreXOracleGetEeIrDirectLoadCounts() - legacy_loads_before;
		const u64 shared_loads_before = EmuCoreXOracleGetEeIrDirectLoadCounts();
		const EESnapshot shared = RunEEProgramWithIrMode(1, code, initial.data(), initial_cp0.data(), nullptr, nullptr, false);
		const u64 shared_loads = EmuCoreXOracleGetEeIrDirectLoadCounts() - shared_loads_before;
		CompareEE(interp, legacy, "GTA branch-only control interpreter/legacy");
		CompareEE(legacy, shared, "GTA branch-only control legacy/shared");
		const auto expected = EeHotLoadExpectedGprs(initial, 2, 0, 0, 0);
		Check(EeHotLoadCheckGprs(interp, expected) && EeHotLoadCheckGprs(legacy, expected) &&
			EeHotLoadCheckGprs(shared, expected),
			"GTA branch-only delay instruction matches independent full-GPR result");
		Check(legacy_loads == 0 && shared_loads == 0,
			"GTA branch-only control admits no direct loads");
		Check(legacy.cycle == shared.cycle, "GTA branch-only control exact legacy/shared cycles");
	}
	EmuCoreXEEOracleSetCompileCallback(nullptr);
	EmuCoreXOracleSetEeIrMode(-1);
}

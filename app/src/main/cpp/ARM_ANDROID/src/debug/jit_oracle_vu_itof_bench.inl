// SPDX-License-Identifier: GPL-3.0+
// Bounded VU ITOF mixed-workload benchmark. Include after jit_oracle_vu_itof.inl.

void VUSharedItofBenchmarks()
{
	const FPControlRegisterBackup callerFPCR(EmuConfig.Cpu.FPUFPCR);
	const auto itof = [](u32 fractional_bits, u32 fs, u32 ft) {
		u32 major = 0x3c;
		switch (fractional_bits)
		{
			case 0: major = 0x3c; break;
			case 4: major = 0x3d; break;
			case 12: major = 0x3e; break;
			case 15: major = 0x3f; break;
		}
		return major | (4u << 6) | (fs << 11) | (ft << 16) | (15u << 21);
	};
	const auto iadd = [](u32 is, u32 it, u32 id) {
		return 0x80000000u | 0x30u | (is << 11) | (it << 16) | (id << 6);
	};
	constexpr std::array<u32, 4> fractions = {0, 4, 12, 15};
	std::array<u32, 128> initial_vf{};
	initial_vf[3] = 0x3f800000u;
	for (u32 reg = 1; reg < 32; ++reg)
		for (u32 lane = 0; lane < 4; ++lane)
			initial_vf[reg * 4 + lane] = kVuItofRawInputs[(reg * 7 + lane * 3) % kVuItofRawInputs.size()];
	std::array<u32, 16> initial_vi{};
	for (u32 reg = 1; reg < 16; ++reg)
		initial_vi[reg] = reg * 0x10203u;

	for (u32 vu = 0; vu < 2; ++vu)
	for (u32 fraction_index = 0; fraction_index < fractions.size(); ++fraction_index)
	{
		FPControlRegister& vu_fpcr = vu ? EmuConfig.Cpu.VU1FPCR : EmuConfig.Cpu.VU0FPCR;
		FPControlRegister& ee_fpcr = EmuConfig.Cpu.FPUFPCR;
		const FPControlRegister saved_vu_fpcr = vu_fpcr;
		const FPControlRegister saved_ee_fpcr = ee_fpcr;
		ee_fpcr.SetRoundMode(FPRoundMode::Nearest);
		vu_fpcr.SetRoundMode(FPRoundMode::Nearest);
		const FPControlRegisterBackup scenario_host_fpcr(ee_fpcr);
		const u32 fraction = fractions[fraction_index];
		std::vector<u32> program;
		for (u32 step = 0; step < 24; ++step)
		{
			const u32 fs = 1 + ((step * 7 + fraction_index) % 31);
			const u32 ft = 1 + ((step * 11 + 9) % 31);
			program.insert(program.end(), {iadd(1 + step % 7, 2 + step % 5, 8 + step % 7),
				itof(fraction, fs, ft)});
		}
		program.insert(program.end(), {NOP_L, NOP_U | 0x40000000u, NOP_L, NOP_U});

		std::array<u32, 128> expected_vf = initial_vf;
		for (u32 step = 0; step < 24; ++step)
		{
			const u32 fs = 1 + ((step * 7 + fraction_index) % 31);
			const u32 ft = 1 + ((step * 11 + 9) % 31);
			for (u32 lane = 0; lane < 4; ++lane)
				expected_vf[ft * 4 + lane] = VuItofExpectedBits(expected_vf[fs * 4 + lane], fraction,
					VuItofRounding::NearestEven);
		}

		std::array<Snapshot, 2> result{};
		std::array<u64, 2> native_bytes{};
		std::array<double, 2> cold_median_us{};
		std::array<double, 2> hot_median_ns{};
		std::array<u64, 2> cold_compile_delta{};
		for (u32 mode = 0; mode < 2; ++mode)
		{
			EmuCoreXOracleSetVUIR(vu, mode ? 1 : 0);
			std::array<double, 7> cold_samples{};
			bool exact_cold_counts = true;
			for (u32 sample = 0; sample < cold_samples.size(); ++sample)
			{
				const u64 before_itof = EmuCoreXOracleVUIRItofInstructions(vu);
				const auto start = std::chrono::steady_clock::now();
				result[mode] = RunVU(vu, true, program, 0x80, 0, 128, false,
					nullptr, initial_vf.data(), nullptr, initial_vi.data());
				cold_samples[sample] = std::chrono::duration<double, std::micro>(
					std::chrono::steady_clock::now() - start).count();
				const u64 itof_delta = EmuCoreXOracleVUIRItofInstructions(vu) - before_itof;
				exact_cold_counts &= itof_delta == (mode ? 24u : 0u);
				cold_compile_delta[mode] = itof_delta;
			}
			std::sort(cold_samples.begin(), cold_samples.end());
			cold_median_us[mode] = cold_samples[cold_samples.size() / 2];
			Check(exact_cold_counts,
				"ITOF benchmark recompiles exactly 24 conversions per fresh shared cold sample");
			Check(std::memcmp(result[mode].regs.VF, expected_vf.data(), sizeof(expected_vf)) == 0,
				"ITOF benchmark final VF matches independent integer-only expected-bit simulation");

			std::vector<JitBlockProfile> profiles;
			if (vu) VU1_JitGetBlockProfiles(profiles); else VU0_JitGetBlockProfiles(profiles);
			bool isolated = !profiles.empty();
			for (const auto& profile : profiles)
			{
				isolated &= profile.startpc >= 0x80 &&
					profile.startpc < 0x80 + program.size() * sizeof(u32);
				native_bytes[mode] += profile.host_size;
			}
			Check(isolated && native_bytes[mode] > 0,
				"ITOF benchmark reports native bytes from bounded program blocks");

			// Measure hot runs while this mode's just-compiled code is still resident.
			// The next mode's cold RunVU resets the shared microVU cache.
			VURegs& regs = vu ? VU1 : VU0;
			BaseVUmicroCPU* cpu = vu ? static_cast<BaseVUmicroCPU*>(&CpuMicroVU1) :
				static_cast<BaseVUmicroCPU*>(&CpuMicroVU0);
			const u64 before_hot_itof = EmuCoreXOracleVUIRItofInstructions(vu);
			const u64 before_hot_ir = EmuCoreXOracleVUIRInstructions(vu);
			std::array<double, 9> hot_samples{};
			bool hot_state = true;
			for (u32 sample = 0; sample < hot_samples.size(); ++sample)
			{
				constexpr u32 iterations = 4096;
				const auto start = std::chrono::steady_clock::now();
				for (u32 iteration = 0; iteration < iterations; ++iteration)
				{
					std::memcpy(regs.VF, initial_vf.data(), sizeof(regs.VF));
					std::memset(regs.VI, 0, sizeof(regs.VI));
					for (u32 reg = 1; reg < initial_vi.size(); ++reg)
						regs.VI[reg].UL = initial_vi[reg];
					regs.cycle = 0;
					regs.VI[REG_TPC].UL = 0x80 / 8;
					VU0.VI[REG_VPU_STAT].UL = vu ? 0x100 : 1;
					cpu->SetStartPC(0x80);
					for (u32 call = 0; call < 128 &&
						(VU0.VI[REG_VPU_STAT].UL & (vu ? 0x100 : 1)); ++call)
						cpu->Execute(128);
					hot_state &= VU0.VI[REG_VPU_STAT].UL == 0 &&
						std::memcmp(regs.VF, expected_vf.data(), sizeof(regs.VF)) == 0 &&
						regs.cycle == result[mode].regs.cycle;
				}
				hot_samples[sample] = std::chrono::duration<double, std::nano>(
					std::chrono::steady_clock::now() - start).count() / iterations;
			}
			std::sort(hot_samples.begin(), hot_samples.end());
			hot_median_ns[mode] = hot_samples[hot_samples.size() / 2];
			Check(hot_state, "ITOF benchmark hot replay preserves independent VF and exact cycles");
			Check(EmuCoreXOracleVUIRItofInstructions(vu) - before_hot_itof == 0 &&
				cold_compile_delta[mode] == (mode ? 24u : 0u),
				"ITOF benchmark hot replay performs no new conversion compiles");
			Check(EmuCoreXOracleVUIRInstructions(vu) == before_hot_ir,
				"ITOF benchmark hot replay performs no new common-IR compiles");
		}
		Compare(result[0], result[1], "ITOF benchmark legacy/shared complete state");
		Check(result[0].regs.cycle == result[1].regs.cycle,
			"ITOF benchmark legacy/shared exact cycle counts match");
		std::printf("VUIRITOFBENCH core=%u fractional_bits=%u itof_ops=24 mixed_iadd_ops=24 cold_samples=7 hot_samples=9 hot_iterations=4096 hot_order=legacy_then_shared legacy_cold_median_us=%.3f shared_cold_median_us=%.3f legacy_hot_median_ns_per_replay=%.3f shared_hot_median_ns_per_replay=%.3f legacy_native_block_bytes=%llu shared_native_block_bytes=%llu\n",
			vu, fraction, cold_median_us[0], cold_median_us[1], hot_median_ns[0], hot_median_ns[1],
			static_cast<unsigned long long>(native_bytes[0]),
			static_cast<unsigned long long>(native_bytes[1]));
		ee_fpcr = saved_ee_fpcr;
		vu_fpcr = saved_vu_fpcr;
	}
	EmuCoreXOracleSetVUIR(0, -1);
	EmuCoreXOracleSetVUIR(1, -1);
}

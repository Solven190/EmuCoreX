// SPDX-License-Identifier: GPL-3.0+
// MicroVuPartial ITOF register-move elimination coverage.
// Include after jit_oracle_vu_itof.inl and call VUBoundItofCodegenTests().

void VUBoundItofCodegenTests()
{
	constexpr std::array<u32, 4> fractions = {0, 4, 12, 15};
	constexpr std::array<VuItofRounding, 4> roundings = {
		VuItofRounding::NearestEven, VuItofRounding::PlusInfinity,
		VuItofRounding::MinusInfinity, VuItofRounding::TowardZero};
	constexpr std::array<FPRoundMode, 4> fpcr_modes = {
		FPRoundMode::Nearest, FPRoundMode::PositiveInfinity,
		FPRoundMode::NegativeInfinity, FPRoundMode::ChopZero};
	bool accepted_all = true;
	bool exact_sizes = true;
	bool exact_results = true;

	for (u32 fraction_index = 0; fraction_index < fractions.size(); ++fraction_index)
	for (u32 mask = 1; mask < 16; ++mask)
	for (u32 alias = 0; alias < 2; ++alias)
	{
		Arm64Ir::BoundVfConvertPlan plan;
		std::string error;
		const ir::Function fn = BoundVfConvertFixture(7, 9, mask, fractions[fraction_index]);
		const bool accepted = ir::Verify(fn, &error) && Arm64Ir::PlanBoundVfConvert(fn, &plan, &error);
		accepted_all &= accepted;
		if (!accepted)
			continue;

		oakSetAsmPtr(SysMemory::GetEERec() + 0x2000000, 0x10000);
		u8* entry = oakStartBlock();
		if (alias)
			oakAsm->LDR(oak::QReg(4), oak::util::X0);
		else
			oakAsm->LDR(oak::QReg(3), oak::util::X0);
		const u8* before = oakGetCurrentCodePointer();
		Arm64Ir::LowerOutput out;
		const bool lowered = Arm64Ir::LowerBoundVfConvertPlan(plan, alias ? 4 : 3, 4,
			&out, &error, Arm64Ir::BoundVfConvertLayout::MicroVuPartial);
		const size_t emitted = static_cast<size_t>(oakGetCurrentCodePointer() - before);
		oakAsm->STR(oak::QReg(4), oak::util::X1);
		oakAsm->RET();
		oakEndBlock();
		accepted_all &= lowered && entry != nullptr && out.frame_size == 0 && out.spill_values == 0;

		const bool one_lane = (mask & (mask - 1)) == 0;
		const bool lane_zero = mask == 8;
		const u32 expected_bytes = one_lane && !lane_zero ? 8u : 4u;
		exact_sizes &= lowered && emitted == expected_bytes && out.host_size == expected_bytes;
		if (!lowered)
			continue;

		for (u32 mode = 0; mode < fpcr_modes.size(); ++mode)
		{
			FPControlRegister fpcr = EmuConfig.Cpu.FPUFPCR;
			fpcr.SetRoundMode(fpcr_modes[mode]);
			const FPControlRegisterBackup restore_fpcr(fpcr);
			for (u32 base = 0; base < kVuItofRawInputs.size(); base += 4)
			{
				std::array<u32, 4> input{};
				std::array<u32, 4> output{};
				for (u32 lane = 0; lane < input.size(); ++lane)
					input[lane] = kVuItofRawInputs[(base + lane) % kVuItofRawInputs.size()];
				reinterpret_cast<void (*)(const u32*, u32*)>(entry)(input.data(), output.data());
				for (u32 lane = 0; lane < 4; ++lane)
				{
					if (!(mask & (8u >> lane)))
						continue;
					const u32 output_lane = one_lane ? 0 : lane;
					exact_results &= output[output_lane] == VuItofExpectedBits(input[lane],
						fractions[fraction_index], roundings[mode]);
				}
			}
		}
	}
	Check(accepted_all, "ITOF codegen optimization fixtures produce valid bounded conversion plans");
	Check(exact_sizes,
		"MicroVuPartial ITOF uses 4 bytes for full/multi/lane-0 masks and 8 for other one-hot masks");
	Check(exact_results,
		"MicroVuPartial ITOF codegen preserves independent raw-bit results in all FPCR modes");
}

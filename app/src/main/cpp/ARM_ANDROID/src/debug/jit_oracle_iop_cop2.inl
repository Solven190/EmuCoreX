// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+

// IOP COP2 register-transfer tests. Include inside the jit_oracle.cpp anonymous
// namespace after the IOP snapshot/program helpers and before IOPTests().
extern "C" u64 EmuCoreXOracleIOPIRCop2Instructions();
namespace
{
    IOPSnapshot IOPCop2ReplayResidentProgram(bool jit, s32 budget, u32 program_words)
    {
        // The program is already resident at IOP_TEST_PC. Restore guest state
        // only: deliberately preserve the interpreter/recompiler caches.
        std::memset(&psxRegs, 0, sizeof(psxRegs));
        for (u32 i = 0; i < 32; ++i)
        {
            psxRegs.CP2D.r[i] = 0x00112233u * (i + 1);
            psxRegs.CP2C.r[i] = 0x44556677u ^ (i * 0x01020304u);
        }
        psxRegs.pc = IOP_TEST_PC;
        psxRegs.cycle = 0;
        psxRegs.iopNextEventCycle = 0x7fffffffu;
        OpcodeFamilies::SetFamilyMask(OpcodeFamilies::CORE_IOP, 0);
        psxCpu = jit ? &psxRec : &psxInt;
        psxCpu->ExecuteBlock(budget);
        IOPSnapshot out;
        CaptureIOP(out);
        const u32 sentinel = IOP_TEST_PC + program_words * 4;
        if (out.pc >= sentinel && out.pc < sentinel + 8)
            out.pc = sentinel;
        return out;
    }

    constexpr u32 IOPCop2Word(u32 rs, u32 rt, u32 rd, u32 alias = 0)
    {
        return (0x12u << 26) | (rs << 21) | (rt << 16) | (rd << 11) | (alias & 0x7ffu);
    }

    void IOPCop2Check(bool pass, const char* format, u32 a, u32 b)
    {
        char name[128];
        std::snprintf(name, sizeof(name), format, a, b);
        Check(pass, name);
    }

    u32 IOPCop2ExpectedClrsb(u32 value)
    {
        const u32 bits = (value & 0x80000000u) ? ~value : value;
        if (bits == 0) return 32;
        u32 count = 0;
        for (u32 mask = 0x80000000u; (bits & mask) == 0; mask >>= 1) ++count;
        return count;
    }

    bool IOPCop2SnapshotMatches(const IOPSnapshot& expected, const IOPSnapshot& actual,
        const char* name)
    {
        bool same = CpuDiff(name, "GPR", expected.gpr, actual.gpr, sizeof(expected.gpr));
        same &= CpuDiff(name, "CP2D", expected.cp2d, actual.cp2d, sizeof(expected.cp2d));
        same &= CpuDiff(name, "CP2C", expected.cp2c, actual.cp2c, sizeof(expected.cp2c));
        Check(same, name);
        return same;
    }

    void IOPCop2ApplyExpected(IOPSnapshot& expected, u32 rs, u32 rt, u32 rd, u32 source)
    {
        switch (rs)
        {
            case 0: // MFC2: the rt==0 early return precedes MFC2(rd), including rd29.
                if (rt != 0)
                {
                    if (rd == 29)
                    {
                        expected.cp2d[29] = ((expected.cp2d[9] >> 7) & 31) |
                            (((expected.cp2d[10] >> 7) & 31) << 5) |
                            (((expected.cp2d[11] >> 7) & 31) << 10);
                    }
                    expected.gpr[rt] = expected.cp2d[rd];
                }
                break;
            case 2: // CFC2
                if (rt != 0) expected.gpr[rt] = expected.cp2c[rd];
                break;
            case 4: // MTC2
                if (rd >= 8 && rd <= 11)
                    expected.cp2d[rd] = static_cast<u32>(static_cast<s32>(static_cast<s16>(source)));
                else if (rd == 15)
                {
                    expected.cp2d[12] = expected.cp2d[13];
                    expected.cp2d[13] = expected.cp2d[14];
                    expected.cp2d[14] = source;
                    expected.cp2d[15] = source;
                }
                else if (rd >= 16 && rd <= 19)
                    expected.cp2d[rd] = source & 0xffffu;
                else if (rd == 28)
                {
                    expected.cp2d[28] = source;
                    expected.cp2d[9] = (source & 31u) << 7;
                    expected.cp2d[10] = ((source >> 5) & 31u) << 7;
                    expected.cp2d[11] = ((source >> 10) & 31u) << 7;
                }
                else if (rd == 30)
                {
                    expected.cp2d[30] = source;
                    expected.cp2d[31] = IOPCop2ExpectedClrsb(source);
                }
                else
                    expected.cp2d[rd] = source;
                break;
            case 6: // CTC2
                expected.cp2c[rd] = source;
                break;
        }
    }

    void IOPCop2FrontendRedTests()
    {
        for (u32 rs : {0u, 2u, 4u, 6u})
            for (u32 rd = 0; rd < 32; ++rd)
            {
                // Bits 10:6 are the only unused alias bits varied here. Keep
                // the low funct field zero: nonzero values can dispatch a GTE
                // command in this core and are not harmless transfer aliases.
                const u32 word = IOPCop2Word(rs, 18, rd, ((rd * 13u) & 31u) << 6);
                ir::Function fn;
                u32 count = 0;
                std::string error;
                const bool lifted = IopIr::LiftSequence(&word, IOP_TEST_PC, 1, fn, &count, &error) && count == 1;
                IOPCop2Check(lifted && Arm64Ir::CanLower(fn, false, &error, Arm64Ir::GuestState::IOP),
                    "RED ir IOP lifts and lowers COP2 transfer rs=%u rd=%u", rs, rd);
            }

        for (u32 rs : {1u, 3u, 5u, 7u, 8u, 15u, 16u, 31u})
        {
            const u32 word = IOPCop2Word(rs, 18, 29, 0x321u);
            ir::Function fn;
            u32 count = 0;
            std::string error;
            IOPCop2Check(!IopIr::LiftSequence(&word, IOP_TEST_PC, 1, fn, &count, &error) && count == 0,
                "ir IOP keeps unsupported COP2 encoding out of shared IR rs=%u rd=%u", rs, 29);
        }

        // In this core the low six bits select GTE command functions even
        // when rs has a transfer encoding. Never admit those as raw transfers.
        for (u32 rs : {0u, 2u, 4u, 6u})
            for (u32 funct : {1u, 0x3fu})
            {
                const u32 word = IOPCop2Word(rs, 18, 29, funct);
                ir::Function fn;
                u32 count = 0;
                std::string error;
                IOPCop2Check(!IopIr::LiftSequence(&word, IOP_TEST_PC, 1, fn, &count, &error) && count == 0,
                    "ir IOP rejects nonzero COP2 command function rs=%u funct=%u", rs, funct);
            }
    }

    void IOPCop2BackendTests()
    {
        for (u32 rd = 0; rd < 32; ++rd)
        {
            ir::Function fn;
            ir::Builder b(fn);
            b.CreateBlock(IOP_TEST_PC);
            const u32 data = b.Emit(ir::Op::ReadCp2Data, ir::Type::I32, {}, rd);
            const u32 control = b.Emit(ir::Op::ReadCp2Control, ir::Type::I32, {}, rd);
            b.Emit1(ir::Op::WriteGpr, ir::Type::Void, data, 8);
            b.Emit1(ir::Op::WriteGpr, ir::Type::Void, control, 9);
            b.Emit1(ir::Op::WriteCp2Data, ir::Type::Void,
                b.Emit(ir::Op::ReadGpr, ir::Type::I32, {}, 10), rd);
            b.Emit1(ir::Op::WriteCp2Control, ir::Type::Void,
                b.Emit(ir::Op::ReadGpr, ir::Type::I32, {}, 11), rd);
            b.Resume(IOP_TEST_PC + 4);
            std::string error;
            bool same = ir::Verify(fn, &error) &&
                Arm64Ir::CanLower(fn, false, &error, Arm64Ir::GuestState::IOP) &&
                !Arm64Ir::CanLower(fn, false, &error, Arm64Ir::GuestState::EE);

            for (bool allocate : {false, true})
            {
                if (!same) break;
                Arm64Ir::LowerOptions options;
                options.guest_state = Arm64Ir::GuestState::IOP;
                options.allocate_registers = allocate;
                Arm64Ir::LowerOutput out;
                same &= Arm64Ir::LowerBlock(fn, options, SysMemory::GetEERec() + 0x2000000,
                    0x10000, &out, &error);
                if (!same) break;

                std::memset(&psxRegs, 0, sizeof(psxRegs));
                std::array<u32, 32> expected_data{}, expected_control{};
                std::array<u32, 34> expected_gpr{};
                for (u32 reg = 0; reg < 32; ++reg)
                {
                    psxRegs.CP2D.r[reg] = expected_data[reg] = 0x10203040u ^ (reg * 0x01010101u);
                    psxRegs.CP2C.r[reg] = expected_control[reg] = 0x89abcdefu + reg * 0x10203u;
                    psxRegs.GPR.r[reg] = expected_gpr[reg] = 0x55667788u ^ (reg * 0x01020304u);
                }
                psxRegs.GPR.r[10] = 0xdeadbeefu;
                psxRegs.GPR.r[11] = 0x76543210u;
                psxRegs.GPR.r[32] = 0x11223344u;
                psxRegs.GPR.r[33] = 0x99aabbccu;
                expected_gpr[10] = psxRegs.GPR.r[10];
                expected_gpr[11] = psxRegs.GPR.r[11];
                expected_gpr[32] = psxRegs.GPR.r[32];
                expected_gpr[33] = psxRegs.GPR.r[33];
                expected_gpr[8] = expected_data[rd];
                expected_gpr[9] = expected_control[rd];
                expected_data[rd] = psxRegs.GPR.r[10];
                expected_control[rd] = psxRegs.GPR.r[11];
                const auto ee_before = cpuRegs;
                reinterpret_cast<void (*)()>(out.entry)();
                same &= std::memcmp(psxRegs.CP2D.r, expected_data.data(), sizeof(expected_data)) == 0 &&
                    std::memcmp(psxRegs.CP2C.r, expected_control.data(), sizeof(expected_control)) == 0 &&
                    std::memcmp(psxRegs.GPR.r, expected_gpr.data(), sizeof(expected_gpr)) == 0 &&
                    std::memcmp(&cpuRegs, &ee_before, sizeof(cpuRegs)) == 0;
            }
            IOPCop2Check(same, "ir IOP raw COP2 backend state/domain rd=%u allocation_modes=%u", rd, 2);
        }

        for (ir::Op op : {ir::Op::ReadCp2Data, ir::Op::ReadCp2Control})
        {
            ir::Function invalid;
            ir::Builder b(invalid);
            b.CreateBlock(IOP_TEST_PC);
            b.Emit(op, ir::Type::I32, {}, 32);
            b.Resume(IOP_TEST_PC + 4);
            std::string error;
            Check(!ir::Verify(invalid, &error), "ir IOP COP2 rejects out-of-range raw read index");
        }
    }

    void IOPCop2IntegrationRedTests()
    {
        struct OverrideReset
        {
            ~OverrideReset() { EmuCoreXOracleSetIOPIR(-1); }
        } override_reset;
        EmuCoreXOracleSetIOPIR(-1);
        IOPCop2BackendTests();

        const std::vector<u32> setup = IOPValueSetup();
        const IOPSnapshot initial = RunIOPProgram(false, setup);
        for (u32 rs : {0u, 2u, 4u, 6u})
            for (u32 rd = 0; rd < 32; ++rd)
            {
                const u32 rt = rs == 4 || rs == 6 ? 8u : 18u;
                const u32 source = initial.gpr[rt];
                std::vector<u32> code = setup;
                code.push_back(IOPCop2Word(rs, rt, rd, ((rd * 7u + rs) & 31u) << 6));
                IOPSnapshot expected = initial;
                IOPCop2ApplyExpected(expected, rs, rt, rd, source);

                EmuCoreXOracleSetIOPIR(0);
                const IOPSnapshot interp = RunIOPProgram(false, code);
                const IOPSnapshot legacy = RunIOPProgram(true, code);
                EmuCoreXOracleSetIOPIR(1);
                const u64 before = EmuCoreXOracleIOPIRCop2Instructions();
                const IOPSnapshot shared = RunIOPProgram(true, code);
                const u64 compiled = EmuCoreXOracleIOPIRCop2Instructions() - before;

                char name[128];
                std::snprintf(name, sizeof(name), "ir IOP COP2 transfer rs=%u rd=%u", rs, rd);
                IOPCop2SnapshotMatches(expected, legacy, "legacy IOP COP2 transfer matches independent expected state");
                IOPCop2SnapshotMatches(expected, shared, name);
                IOPCop2SnapshotMatches(expected, interp, "interpreter IOP COP2 transfer matches independent expected state");
                IOPCop2Check(interp.cycle == legacy.cycle && legacy.cycle == shared.cycle,
                    "ir IOP COP2 exact interpreter/legacy cycle rs=%u rd=%u", rs, rd);
                IOPCop2Check(compiled == 1,
                    "ir IOP COP2 transfer compiled rs=%u rd=%u", rs, rd);
            }

        // MFC2 rt==$0 returns before the rd29 ORGB read-side effect.
        const u32 expectedRgb = ((initial.cp2d[9] >> 7) & 31) |
            (((initial.cp2d[10] >> 7) & 31) << 5) |
            (((initial.cp2d[11] >> 7) & 31) << 10);
        Check(initial.cp2d[29] != expectedRgb,
            "ir IOP COP2 rd29 zero-target fixture distinguishes preserved ORGB from recomputed value");
        for (u32 rs : {0u, 2u})
        {
            std::vector<u32> code = setup;
            code.push_back(IOPCop2Word(rs, 0, 29, 0x7c0u));
            const IOPSnapshot expected = initial;
            EmuCoreXOracleSetIOPIR(1);
            const u64 before = EmuCoreXOracleIOPIRCop2Instructions();
            const IOPSnapshot actual = RunIOPProgram(true, code);
            IOPCop2SnapshotMatches(expected, actual, "ir IOP COP2 read into zero preserves all GTE/GPR state");
            IOPCop2Check(EmuCoreXOracleIOPIRCop2Instructions() - before == 1,
                "ir IOP COP2 zero-target transfer compiled rs=%u rd=%u", rs, 29);
        }

        // Writes sourced from $0 still update the selected CP2 register.
        for (u32 rs : {4u, 6u})
            for (u32 rd = 0; rd < 32; ++rd)
            {
                std::vector<u32> code = setup;
                code.push_back(IOPCop2Word(rs, 0, rd, ((rd + 1u) & 31u) << 6));
                IOPSnapshot expected = initial;
                IOPCop2ApplyExpected(expected, rs, 0, rd, 0);
                EmuCoreXOracleSetIOPIR(0);
                const IOPSnapshot interp = RunIOPProgram(false, code);
                const IOPSnapshot legacy = RunIOPProgram(true, code);
                EmuCoreXOracleSetIOPIR(1);
                const u64 before = EmuCoreXOracleIOPIRCop2Instructions();
                const IOPSnapshot actual = RunIOPProgram(true, code);
                IOPCop2SnapshotMatches(expected, interp, "interpreter IOP COP2 zero-source write independent state");
                IOPCop2SnapshotMatches(expected, legacy, "legacy IOP COP2 zero-source write independent state");
                IOPCop2SnapshotMatches(expected, actual, "ir IOP COP2 zero-source write independent state");
                IOPCop2Check(interp.cycle == legacy.cycle && legacy.cycle == actual.cycle,
                    "ir IOP COP2 zero-source exact cycle rs=%u rd=%u", rs, rd);
                IOPCop2Check(EmuCoreXOracleIOPIRCop2Instructions() - before == 1,
                    "ir IOP COP2 zero-source write compiled rs=%u rd=%u", rs, rd);
            }

        // Exercise sign, RGB packing, and LZCR edge behavior with source
        // values independent of the default fixture's single register value.
        for (u32 rd : {8u, 9u, 10u, 11u, 28u, 30u})
            for (u32 value : {0u, 1u, 0x00007fffu, 0xffff8001u, 0x7fffffffu,
                0x80000000u, 0xfffffffeu, 0xffffffffu, 0xdeadbeefu})
            {
                std::vector<u32> code = setup;
                code.push_back(MipsI(15, 0, 8, value >> 16));
                code.push_back(MipsI(13, 8, 8, value));
                const IOPSnapshot before_state = RunIOPProgram(false, code);
                code.push_back(IOPCop2Word(4, 8, rd));
                IOPSnapshot expected = before_state;
                IOPCop2ApplyExpected(expected, 4, 8, rd, value);

                EmuCoreXOracleSetIOPIR(0);
                const IOPSnapshot interp = RunIOPProgram(false, code);
                const IOPSnapshot legacy = RunIOPProgram(true, code);
                EmuCoreXOracleSetIOPIR(1);
                const u64 before = EmuCoreXOracleIOPIRCop2Instructions();
                const IOPSnapshot actual = RunIOPProgram(true, code);
                IOPCop2SnapshotMatches(expected, interp, "interpreter IOP COP2 special-write edge independent state");
                IOPCop2SnapshotMatches(expected, legacy, "legacy IOP COP2 special-write edge independent state");
                IOPCop2SnapshotMatches(expected, actual, "ir IOP COP2 special-write edge independent state");
                IOPCop2Check(interp.cycle == legacy.cycle && legacy.cycle == actual.cycle,
                    "ir IOP COP2 special-write edge exact cycle rd=%u value=%u", rd, value);
                IOPCop2Check(EmuCoreXOracleIOPIRCop2Instructions() - before == 1,
                    "ir IOP COP2 special-write edge compiled rd=%u value=%u", rd, value);
            }

        // A COP2 transfer in the delay slot executes for both branch outcomes.
        for (bool taken : {false, true})
            for (u32 rs : {0u, 2u, 4u, 6u})
            {
                const u32 rd = rs == 0 ? 29 : rs == 4 ? 15 : 7;
                const u32 rt = rs == 0 || rs == 2 ? 18 : 8;
                std::vector<u32> code = setup;
                code.push_back(taken ? MipsI(4, 0, 0, 1) : MipsI(5, 0, 0, 1));
                code.push_back(IOPCop2Word(rs, rt, rd));
                const IOPSnapshot before_state = RunIOPProgram(false, setup);
                IOPSnapshot expected = before_state;
                IOPCop2ApplyExpected(expected, rs, rt, rd, before_state.gpr[8]);

                EmuCoreXOracleSetIOPIR(0);
                const IOPSnapshot interp = RunIOPProgram(false, code);
                const IOPSnapshot legacy = RunIOPProgram(true, code);
                EmuCoreXOracleSetIOPIR(1);
                const u64 before = EmuCoreXOracleIOPIRCop2Instructions();
                const IOPSnapshot shared = RunIOPProgram(true, code);
                IOPCop2SnapshotMatches(expected, legacy, "legacy IOP COP2 delay transfer independent state");
                IOPCop2SnapshotMatches(expected, shared, "ir IOP COP2 delay transfer independent state");
                IOPCop2SnapshotMatches(expected, interp, "interpreter IOP COP2 delay transfer independent state");
                IOPCop2Check(interp.cycle == legacy.cycle && legacy.cycle == shared.cycle,
                    "ir IOP COP2 branch delay exact cycle taken=%u rs=%u", taken, rs);
                IOPCop2Check(EmuCoreXOracleIOPIRCop2Instructions() - before == 1,
                    "ir IOP COP2 delay transfer compiled taken=%u rs=%u", taken, rs);
            }

        // A fixed-size transfer-heavy run provides a bounded code-size and
        // dispatch benchmark while retaining exact state/cycle checks.
        std::vector<u32> benchmark = setup;
        constexpr u32 transfer_groups = 64;
        for (u32 i = 0; i < transfer_groups; ++i)
        {
            const u32 rd = (i * 7u + 3u) & 31u;
            benchmark.push_back(IOPCop2Word(4, 8, rd)); // MTC2
            benchmark.push_back(IOPCop2Word(0, 18, rd)); // MFC2
            benchmark.push_back(IOPCop2Word(6, 9, rd)); // CTC2
            benchmark.push_back(IOPCop2Word(2, 19, rd)); // CFC2
        }
        const s32 benchmark_budget = static_cast<s32>((benchmark.size() + 16) * 8);
        const auto interp_start = std::chrono::steady_clock::now();
        const IOPSnapshot bench_interp = RunIOPProgram(false, benchmark, benchmark_budget);
        const double interp_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - interp_start).count();
        EmuCoreXOracleSetIOPIR(0);
        const auto legacy_start = std::chrono::steady_clock::now();
        const IOPSnapshot bench_legacy = RunIOPProgram(true, benchmark, benchmark_budget);
        const double legacy_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - legacy_start).count();
        EmuCoreXOracleSetIOPIR(1);
        const u64 benchmark_ir_before = EmuCoreXOracleIOPIRInstructions();
        const u64 benchmark_cop2_before = EmuCoreXOracleIOPIRCop2Instructions();
        const auto shared_start = std::chrono::steady_clock::now();
        const IOPSnapshot bench_shared = RunIOPProgram(true, benchmark, benchmark_budget);
        const double shared_us = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - shared_start).count();
        const u64 compiled = EmuCoreXOracleIOPIRInstructions() - benchmark_ir_before;
        const u64 compiled_cop2 = EmuCoreXOracleIOPIRCop2Instructions() - benchmark_cop2_before;
        IOPCop2SnapshotMatches(bench_interp, bench_legacy, "transfer benchmark legacy state matches interpreter");
        IOPCop2SnapshotMatches(bench_interp, bench_shared, "transfer benchmark shared state matches interpreter");
        IOPCop2Check(bench_interp.cycle == bench_legacy.cycle && bench_legacy.cycle == bench_shared.cycle,
            "ir IOP COP2 transfer benchmark exact cycles groups=%u code_words=%u",
            transfer_groups, static_cast<u32>(benchmark.size()));
        IOPCop2Check(compiled_cop2 == transfer_groups * 4,
            "ir IOP COP2 transfer benchmark compiled transfers groups=%u code_words=%u",
            transfer_groups, static_cast<u32>(benchmark.size()));

        // Warm each engine once, then replay resident code seven times with
        // only guest state reset. Report median reset+execute time; compilation
        // and code writes are outside these timed windows.
        constexpr u32 hot_samples = 7;
        std::array<double, hot_samples> interp_hot_times{}, legacy_hot_times{}, shared_hot_times{};
        IOPSnapshot interp_hot{}, legacy_hot{}, shared_hot{};
        const auto replay_samples = [&](bool jit, bool shared_ir,
            std::array<double, hot_samples>& times, IOPSnapshot& last) {
            EmuCoreXOracleSetIOPIR(shared_ir ? 1 : 0);
            for (u32 sample = 0; sample < hot_samples; ++sample)
            {
                const auto start = std::chrono::steady_clock::now();
                last = IOPCop2ReplayResidentProgram(jit, benchmark_budget,
                    static_cast<u32>(benchmark.size()));
                times[sample] = std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - start).count();
            }
        };
        replay_samples(false, false, interp_hot_times, interp_hot);

        // Recompile the legacy cache, then keep it intact for its hot samples.
        EmuCoreXOracleSetIOPIR(0);
        const IOPSnapshot legacy_warm = RunIOPProgram(true, benchmark, benchmark_budget);
        replay_samples(true, false, legacy_hot_times, legacy_hot);
        IOPCop2SnapshotMatches(bench_interp, legacy_warm, "legacy benchmark warm state matches interpreter");

        // RunIOPProgram resets the recompiler cache; this prepares a distinct
        // shared-IR cache before measuring shared hot execution.
        EmuCoreXOracleSetIOPIR(1);
        const IOPSnapshot shared_warm = RunIOPProgram(true, benchmark, benchmark_budget);
        IOPCop2SnapshotMatches(bench_interp, shared_warm, "shared benchmark warm state matches interpreter");
        const u64 hot_compile_before = EmuCoreXOracleIOPIRInstructions();
        const u64 hot_cop2_before = EmuCoreXOracleIOPIRCop2Instructions();
        replay_samples(true, true, shared_hot_times, shared_hot);
        IOPCop2Check(EmuCoreXOracleIOPIRInstructions() == hot_compile_before &&
            EmuCoreXOracleIOPIRCop2Instructions() == hot_cop2_before,
            "ir IOP COP2 hot replay performs no compilation groups=%u code_words=%u",
            transfer_groups, static_cast<u32>(benchmark.size()));
        IOPCop2SnapshotMatches(bench_interp, interp_hot, "interpreter hot replay state matches cold run");
        IOPCop2SnapshotMatches(bench_interp, legacy_hot, "legacy hot replay state matches cold run");
        IOPCop2SnapshotMatches(bench_interp, shared_hot, "shared hot replay state matches cold run");
        IOPCop2Check(interp_hot.cycle == legacy_hot.cycle && legacy_hot.cycle == shared_hot.cycle,
            "ir IOP COP2 hot replay exact cycles groups=%u code_words=%u",
            transfer_groups, static_cast<u32>(benchmark.size()));
        const auto median_us = [](std::array<double, hot_samples> values) {
            std::sort(values.begin(), values.end());
            return values[hot_samples / 2];
        };
        const double interp_hot_us = median_us(interp_hot_times);
        const double legacy_hot_us = median_us(legacy_hot_times);
        const double shared_hot_us = median_us(shared_hot_times);
        std::vector<JitBlockProfile> profiles;
        IOP_JitGetBlockProfiles(profiles);
        u64 native_bytes = 0;
        bool bounded = !profiles.empty();
        for (const JitBlockProfile& profile : profiles)
        {
            bounded &= profile.host_size < 0x10000;
            native_bytes += profile.host_size;
        }
        IOPCop2Check(bounded, "ir IOP COP2 transfer benchmark block bounds groups=%u code_words=%u",
            transfer_groups, static_cast<u32>(benchmark.size()));
        std::printf("IOPCOP2BENCH groups=%u code_words=%zu cold_interp_reset_setup_execute_us=%.3f cold_legacy_reset_setup_compile_execute_us=%.3f cold_shared_reset_setup_compile_execute_us=%.3f hot_interp_median_reset_execute_us=%.3f hot_legacy_median_reset_execute_us=%.3f hot_shared_median_reset_execute_us=%.3f hot_samples=%u native_block_bytes=%llu compiled_words=%llu compiled_cop2=%llu\n",
            transfer_groups, benchmark.size(), interp_us, legacy_us, shared_us,
            interp_hot_us, legacy_hot_us, shared_hot_us, hot_samples,
            static_cast<unsigned long long>(native_bytes), static_cast<unsigned long long>(compiled),
            static_cast<unsigned long long>(compiled_cop2));
    }
}

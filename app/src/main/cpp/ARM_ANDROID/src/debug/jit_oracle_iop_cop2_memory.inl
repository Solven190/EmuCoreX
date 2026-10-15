// SPDX-License-Identifier: GPL-3.0+
// IOP LWC2/SWC2 coverage. Include after jit_oracle_iop_cop2.inl so its
// independent COP2 expected-state helpers are available.
extern "C" u64 EmuCoreXOracleIOPIRMemoryOperations();

namespace
{
    constexpr u32 IOPCop2MemoryWord(u32 opcode, u32 base, u32 reg, u32 offset)
    {
        return MipsI(opcode, base, reg, offset);
    }

    constexpr u32 IOPCop2MemoryLoadOpcode = 50;
    constexpr u32 IOPCop2MemoryStoreOpcode = 58;
    constexpr u32 IOPCop2MemoryPage3Register = 0x1f803100;

    std::vector<u32> IOPCop2MemoryBaseSetup(u32 base_reg, u32 base)
    {
        if (base_reg == 0)
            return {};
        return {MipsI(15, 0, base_reg, base >> 16), MipsI(13, base_reg, base_reg, base)};
    }

    u32 IOPCop2MemoryEffectiveAddress(u32 base, u32 offset)
    {
        return (base + static_cast<u32>(static_cast<s32>(static_cast<s16>(offset)))) & 0x1fffffffu;
    }

    u32 IOPCop2MemoryReadExpected(const IOPSnapshot& before, u32 address)
    {
        u32 value = 0;
        for (u32 byte = 0; byte < 4; ++byte)
        {
            const u32 current = address + byte;
            u8 part = 0;
            if (current >= IOP_TEST_SCRATCH && current < IOP_TEST_SCRATCH + IOP_SCRATCH_SIZE)
                part = before.scratch[current - IOP_TEST_SCRATCH];
            else
                part = iopMemRead8(current);
            value |= static_cast<u32>(part) << (byte * 8);
        }
        return value;
    }

    void IOPCop2MemoryStoreExpected(IOPSnapshot& expected, u32 address, u32 value)
    {
        for (u32 byte = 0; byte < 4; ++byte)
        {
            const u32 current = address + byte;
            if (current >= IOP_TEST_SCRATCH && current < IOP_TEST_SCRATCH + IOP_SCRATCH_SIZE)
                expected.scratch[current - IOP_TEST_SCRATCH] = static_cast<u8>(value >> (byte * 8));
        }
    }

    u32 IOPCop2MemoryStoreSource(IOPSnapshot& expected, u32 reg)
    {
        if (reg != 29)
            return expected.cp2d[reg];
        expected.cp2d[29] = ((expected.cp2d[9] >> 7) & 31u) |
            (((expected.cp2d[10] >> 7) & 31u) << 5) |
            (((expected.cp2d[11] >> 7) & 31u) << 10);
        return expected.cp2d[29];
    }

    void IOPCop2MemoryFrontendRedTests()
    {
        for (u32 opcode : {IOPCop2MemoryLoadOpcode, IOPCop2MemoryStoreOpcode})
            for (u32 reg = 0; reg < 32; ++reg)
            {
                const u32 word = IOPCop2MemoryWord(opcode, 8, reg, 0xfffdu);
                ir::Function fn;
                std::string error;
                u32 count = 0;
                const bool lifted = IopIr::LiftSequence(&word, IOP_TEST_PC, 1, fn, &count, &error) && count == 1;
                bool has_memory = false;
                u32 memory_ops = 0;
                for (const auto& block : fn.blocks)
                    for (const auto& inst : block.insts)
                    {
                        if (inst.op == ir::Op::Load32 || inst.op == ir::Op::Store32)
                        {
                            has_memory = true;
                            ++memory_ops;
                        }
                    }
                Arm64Ir::LowerOptions options;
                options.guest_state = Arm64Ir::GuestState::IOP;
                Arm64Ir::LowerOutput out;
                const bool lowerable = lifted && Arm64Ir::CanLower(fn, false, &error, Arm64Ir::GuestState::IOP) &&
                    Arm64Ir::LowerBlock(fn, options, SysMemory::GetEERec() + 0x2000000, 0x10000, &out, &error);
                IOPCop2Check(lifted && has_memory && memory_ops == 1 && lowerable && out.iop_memory_operations == 1,
                    "ir IOP COP2 memory frontend opcode=%u target=%u", opcode, reg);
            }
    }

    void IOPCop2MemoryStateCase(u32 opcode, u32 reg, u32 base_reg, u32 base, u32 offset,
        const u32* expected_read_override = nullptr)
    {
        const std::vector<u32> setup = IOPCop2MemoryBaseSetup(base_reg, base);
        const IOPSnapshot before = RunIOPProgram(false, setup);
        const u32 address = IOPCop2MemoryEffectiveAddress(base, offset);
        std::vector<u32> code = setup;
        code.push_back(IOPCop2MemoryWord(opcode, base_reg, reg, offset));

        IOPSnapshot expected = before;
        if (opcode == IOPCop2MemoryLoadOpcode)
        {
            const u32 value = expected_read_override ? *expected_read_override :
                IOPCop2MemoryReadExpected(before, address);
            IOPCop2ApplyExpected(expected, 4, 8, reg, value);
        }
        else
        {
            const u32 value = IOPCop2MemoryStoreSource(expected, reg);
            IOPCop2MemoryStoreExpected(expected, address, value);
        }
        const bool check_external_ram_store = opcode == IOPCop2MemoryStoreOpcode && address < 0x00800000u &&
            (address < IOP_TEST_SCRATCH || address + 3 >= IOP_TEST_SCRATCH + IOP_SCRATCH_SIZE);
        const bool check_page3_store = opcode == IOPCop2MemoryStoreOpcode && address == IOPCop2MemoryPage3Register;
        const u32 expected_store_value = reg == 29 ? expected.cp2d[29] : before.cp2d[reg];
        const u32 page3_initial = 0xa5a51234u;
        const u32 expected_page3_word = (page3_initial & 0xffff0000u) | (expected_store_value & 0xffffu);
        const auto reset_external_store_target = [&]() {
            if (check_external_ram_store)
                iopMemWrite32(address, 0x5aa55aa5u);
            if (check_page3_store)
                psxHu32(address) = page3_initial;
        };
        const auto check_store_target = [&](u32 engine) {
            if (check_external_ram_store)
                IOPCop2Check(iopMemRead32(address) == expected_store_value,
                    "ir IOP COP2 RAM store engine=%u target=%u", engine, reg);
            if (check_page3_store)
                IOPCop2Check(psxHu32(address) == expected_page3_word,
                    "ir IOP COP2 MMIO store engine=%u target=%u", engine, reg);
        };
        expected.pc = IOP_TEST_PC + static_cast<u32>(code.size()) * 4;

        EmuCoreXOracleSetIOPIR(0);
        reset_external_store_target();
        const IOPSnapshot interp = RunIOPProgram(false, code);
        check_store_target(0);
        reset_external_store_target();
        const IOPSnapshot legacy = RunIOPProgram(true, code);
        check_store_target(1);
        EmuCoreXOracleSetIOPIR(1);
        const u64 memory_before = EmuCoreXOracleIOPIRMemoryOperations();
        reset_external_store_target();
        const IOPSnapshot shared = RunIOPProgram(true, code);
        check_store_target(2);
        const u64 compiled_memory = EmuCoreXOracleIOPIRMemoryOperations() - memory_before;
        const u64 forced_before = EmuCoreXOracleIOPIRMemoryOperations();
        reset_external_store_target();
        const IOPSnapshot forced = RunIOPProgram(true, code, -1, false,
            1ull << OpcodeFamilies::IOP::FAM_LOADSTORE);
        check_store_target(3);

        char name[144];
        std::snprintf(name, sizeof(name), "ir IOP COP2 memory opcode=%u target=%u base=%08x off=%04x",
            opcode, reg, base, offset & 0xffffu);
        CompareIOP(expected, interp, "interpreter LWC2/SWC2 matches independent expected state");
        CompareIOP(expected, legacy, "legacy LWC2/SWC2 matches independent expected state");
        CompareIOP(expected, shared, name);
        CompareIOP(expected, forced, "forced GTE fallback LWC2/SWC2 matches independent expected state");
        IOPCop2Check(interp.cycle == legacy.cycle && legacy.cycle == shared.cycle && shared.cycle == forced.cycle,
            "ir IOP COP2 memory exact cycles opcode=%u target=%u", opcode, reg);
        IOPCop2Check(compiled_memory > 0,
            "ir IOP COP2 memory transfer entered shared IR opcode=%u target=%u", opcode, reg);
        IOPCop2Check(EmuCoreXOracleIOPIRMemoryOperations() == forced_before,
                            "ir IOP COP2 forced LOADSTORE memory fallback stays interpreted opcode=%u target=%u", opcode, reg);
    }

    void IOPCop2MemoryRegisterAndOffsetRedTests()
    {
        for (u32 opcode : {IOPCop2MemoryLoadOpcode, IOPCop2MemoryStoreOpcode})
            for (u32 reg = 0; reg < 32; ++reg)
                for (u32 offset = 0; offset < 4; ++offset)
                    IOPCop2MemoryStateCase(opcode, reg, 8, IOP_TEST_SCRATCH + 64, offset);

        // Signed displacements reach the same four unaligned words through a
        // different base, exposing unsigned-immediate and address-fold errors.
        for (u32 opcode : {IOPCop2MemoryLoadOpcode, IOPCop2MemoryStoreOpcode})
            for (u32 reg : {0u, 8u, 15u, 28u, 29u, 30u, 31u})
                for (u32 align = 0; align < 4; ++align)
                    IOPCop2MemoryStateCase(opcode, reg, 8, IOP_TEST_SCRATCH + 68, 0xfffcu + align);
    }

    void IOPCop2MemoryAliasBoundaryRedTests()
    {
        for (u32 opcode : {IOPCop2MemoryLoadOpcode, IOPCop2MemoryStoreOpcode})
            for (u32 alias : {0u, 0x80000000u, 0xa0000000u, 0x20000000u, 0xe0000000u})
                for (u32 reg : {0u, 15u, 28u, 29u, 30u, 31u})
                    for (u32 offset = 0; offset < 4; ++offset)
                        IOPCop2MemoryStateCase(opcode, reg, 8,
                            alias | (IOP_TEST_SCRATCH + 64), offset);

        constexpr u32 boundary = IOP_TEST_SCRATCH + 0xfffcu;
        for (u32 byte = 0; byte < 8; ++byte)
            iopMemWrite8(boundary + byte, static_cast<u8>(0x31u + byte * 17u));
        for (u32 opcode : {IOPCop2MemoryLoadOpcode, IOPCop2MemoryStoreOpcode})
            for (u32 reg : {8u, 15u, 29u, 30u})
                IOPCop2MemoryStateCase(opcode, reg, 8, boundary, 3);

        // GPR $0 addressing and 32-bit effective-address wrap both land in
        // low RAM. Seed it outside the guest program to keep the model distinct.
        for (u32 opcode : {IOPCop2MemoryLoadOpcode, IOPCop2MemoryStoreOpcode})
        {
            iopMemWrite32(0x00001234, 0x89abcdefu);
            IOPCop2MemoryStateCase(opcode, 30, 0, 0, 0x1234);
            iopMemWrite32(0x00000010, 0x76543210u);
            IOPCop2MemoryStateCase(opcode, 29, 8, 0xfffffff0u, 0x20);
        }
    }

    struct IOPCop2MemoryClearRecord
    {
        void (*original)(u32, u32) = nullptr;
        u32 address = 0;
        u32 count = 0;
        u32 pc = 0;
        u32 code = 0;
        u32 expected_pc = 0;
        u32 expected_code = 0;
        u32 size = 0;
        u32 value_after_store = 0;
        u32 cp2d29_at_callback = 0;
    };
    IOPCop2MemoryClearRecord s_iopCop2MemoryClearRecord;

    void IOPCop2MemoryClearProbe(u32 address, u32 size)
    {
        if (address == s_iopCop2MemoryClearRecord.address &&
            psxRegs.pc == s_iopCop2MemoryClearRecord.expected_pc &&
            psxRegs.code == s_iopCop2MemoryClearRecord.expected_code)
        {
            ++s_iopCop2MemoryClearRecord.count;
            s_iopCop2MemoryClearRecord.pc = psxRegs.pc;
            s_iopCop2MemoryClearRecord.code = psxRegs.code;
            s_iopCop2MemoryClearRecord.size = size;
            s_iopCop2MemoryClearRecord.value_after_store = iopMemRead32(address);
            s_iopCop2MemoryClearRecord.cp2d29_at_callback = psxRegs.CP2D.r[29];
        }
        s_iopCop2MemoryClearRecord.original(address, size);
    }

    void IOPCop2MemoryMMIOAndHookRedTests()
    {
        // Page 3 is a deterministic MMIO backing page. It exercises the
        // helper path while avoiding timer, interrupt, and DMA side effects.
        const u32 saved_mmio = psxHu32(IOPCop2MemoryPage3Register);
        constexpr u32 initial_mmio = 0xa5a51234u;
        psxHu32(IOPCop2MemoryPage3Register) = initial_mmio;
        for (u32 rt : {8u, 15u, 28u, 29u, 30u})
        {
            IOPCop2MemoryStateCase(IOPCop2MemoryLoadOpcode, rt, 8,
                IOPCop2MemoryPage3Register, 0, &initial_mmio);
        }
        for (u32 rt : {8u, 15u, 28u, 29u, 30u})
        {
            psxHu32(IOPCop2MemoryPage3Register) = initial_mmio;
            IOPCop2MemoryStateCase(IOPCop2MemoryStoreOpcode, rt, 8,
                IOPCop2MemoryPage3Register, 0);
            const u32 stored = rt == 29 ?
                (((0x00112233u * 10u >> 7) & 31u) | (((0x00112233u * 11u >> 7) & 31u) << 5) |
                    (((0x00112233u * 12u >> 7) & 31u) << 10)) : 0x00112233u * (rt + 1);
            IOPCop2Check(psxHu32(IOPCop2MemoryPage3Register) == ((initial_mmio & 0xffff0000u) | (stored & 0xffffu)),
                "ir IOP COP2 MMIO store follows page3 handler target=%u value=%u", rt, stored);
        }
        psxHu32(IOPCop2MemoryPage3Register) = saved_mmio;

        // A RAM store invokes the normal JIT invalidation callback after the
        // helper sees the guest PC/opcode and after the store updates memory.
        for (u32 opcode : {IOPCop2MemoryLoadOpcode, IOPCop2MemoryStoreOpcode})
            for (bool delay_slot : {false, true})
                for (u32 outcome = 0; outcome < (delay_slot ? 2u : 1u); ++outcome)
                {
                    const bool taken = outcome != 0;
                    const u32 reg = opcode == IOPCop2MemoryLoadOpcode ? 15u : 29u;
                    std::vector<u32> code = IOPCop2MemoryBaseSetup(8, IOP_TEST_SCRATCH + 64);
                    if (delay_slot)
                        code.push_back(taken ? MipsI(4, 0, 0, 1) : MipsI(5, 0, 0, 1));
                    const u32 memory_word = IOPCop2MemoryWord(opcode, 8, reg, 0);
                    code.push_back(memory_word);
                    const IOPSnapshot before = RunIOPProgram(false, IOPCop2MemoryBaseSetup(8, IOP_TEST_SCRATCH + 64));
                    IOPSnapshot expected = before;
                    if (opcode == IOPCop2MemoryLoadOpcode)
                        IOPCop2ApplyExpected(expected, 4, 8, reg,
                            IOPCop2MemoryReadExpected(before, IOP_TEST_SCRATCH + 64));
                    else
                    {
                        const u32 value = IOPCop2MemoryStoreSource(expected, reg);
                        IOPCop2MemoryStoreExpected(expected, IOP_TEST_SCRATCH + 64, value);
                    }
                    expected.pc = IOP_TEST_PC + static_cast<u32>(code.size()) * 4;

                    EmuCoreXOracleSetIOPIR(0);
                    const IOPSnapshot interp = RunIOPProgram(false, code);
                    const IOPSnapshot legacy = RunIOPProgram(true, code);
                    EmuCoreXOracleSetIOPIR(1);
                    const u64 memory_before = EmuCoreXOracleIOPIRMemoryOperations();
                    s_iopCop2MemoryClearRecord = {};
                    if (opcode == IOPCop2MemoryStoreOpcode)
                    {
                        s_iopCop2MemoryClearRecord.original = psxRec.Clear;
                        s_iopCop2MemoryClearRecord.address = (IOP_TEST_SCRATCH + 64) & ~3u;
                        s_iopCop2MemoryClearRecord.expected_pc = IOP_TEST_PC + static_cast<u32>(code.size()) * 4;
                        s_iopCop2MemoryClearRecord.expected_code = memory_word;
                        psxRec.Clear = &IOPCop2MemoryClearProbe;
                    }
                    const IOPSnapshot shared = RunIOPProgram(true, code);
                    if (opcode == IOPCop2MemoryStoreOpcode)
                        psxRec.Clear = s_iopCop2MemoryClearRecord.original;
                    const u64 compiled_memory = EmuCoreXOracleIOPIRMemoryOperations() - memory_before;
                    const u64 forced_before = EmuCoreXOracleIOPIRMemoryOperations();
                    const IOPSnapshot forced = RunIOPProgram(true, code, -1, false,
                        1ull << OpcodeFamilies::IOP::FAM_LOADSTORE);
                    CompareIOP(expected, interp, "interpreter COP2 memory branch-delay independent state");
                    CompareIOP(expected, legacy, "legacy COP2 memory branch-delay independent state");
                    CompareIOP(expected, shared, "shared COP2 memory branch-delay independent state");
                    CompareIOP(expected, forced, "forced LOADSTORE COP2 memory branch-delay independent state");
                    IOPCop2Check(interp.cycle == legacy.cycle && legacy.cycle == shared.cycle &&
                        shared.cycle == forced.cycle && compiled_memory > 0 &&
                        EmuCoreXOracleIOPIRMemoryOperations() == forced_before,
                        "ir IOP COP2 memory branch-delay cycles/compile opcode=%u taken=%u", opcode, taken ? 1u : 0u);
                    if (opcode == IOPCop2MemoryStoreOpcode)
                    {
                        const u32 expected_orbg = ((0x00112233u * 10u >> 7) & 31u) |
                            (((0x00112233u * 11u >> 7) & 31u) << 5) |
                            (((0x00112233u * 12u >> 7) & 31u) << 10);
                        const u32 guest_pc = IOP_TEST_PC + static_cast<u32>(code.size() - 1) * 4;
                        if (!(s_iopCop2MemoryClearRecord.count == 1 &&
                            s_iopCop2MemoryClearRecord.pc == guest_pc + 4 &&
                            s_iopCop2MemoryClearRecord.code == memory_word &&
                            s_iopCop2MemoryClearRecord.size == 1 &&
                            s_iopCop2MemoryClearRecord.value_after_store == expected_orbg &&
                            s_iopCop2MemoryClearRecord.cp2d29_at_callback == expected_orbg))
                        {
                            std::printf("SWC2 hook delay=%u taken=%u count=%u pc=%08x/%08x code=%08x/%08x size=%u value=%08x cp2d29=%08x expected=%08x\n",
                                delay_slot ? 1u : 0u, taken ? 1u : 0u,
                                s_iopCop2MemoryClearRecord.count, s_iopCop2MemoryClearRecord.pc, guest_pc + 4,
                                s_iopCop2MemoryClearRecord.code, memory_word, s_iopCop2MemoryClearRecord.size,
                                s_iopCop2MemoryClearRecord.value_after_store,
                                s_iopCop2MemoryClearRecord.cp2d29_at_callback, expected_orbg);
                            IOPCop2Check(false, "ir IOP COP2 SWC2 hook delay/taken=%u/%u",
                                delay_slot ? 1u : 0u, taken ? 1u : 0u);
                        }
                    }
                }
        EmuCoreXOracleSetIOPIR(-1);
    }

    void IOPCop2MemoryIntegrationRedTests()
    {
        struct OverrideReset
        {
            ~OverrideReset() { EmuCoreXOracleSetIOPIR(-1); }
        } override_reset;
        IOPCop2MemoryFrontendRedTests();
        IOPCop2MemoryRegisterAndOffsetRedTests();
        IOPCop2MemoryAliasBoundaryRedTests();
        IOPCop2MemoryMMIOAndHookRedTests();
    }
}

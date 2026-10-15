// SPDX-License-Identifier: GPL-3.0+
// EE fastmem ABI proof using a callable wrapper in the EE code arena.
namespace
{
    constexpr u32 EEFastmemAbiAlias = 0x46000000u;
    constexpr u32 EEFastmemAbiMmio = 0x1fc00000u;
    constexpr u32 EEFastmemAbiPage = vtlb_private::VTLB_PAGE_SIZE;
    constexpr u32 EEFastmemAbiPc = EE_TEST_PC + 0x80;

    struct EEFastmemAbiTrace
    {
        u32 calls = 0;
        u32 address = 0;
        u32 pc = 0;
        u32 delay_slot = 0;
        u32 cycle_before = 0;
        u32 cycle_after = 0;
    };

    EEFastmemAbiTrace s_ee_fastmem_abi_trace;

    u32 EEFastmemAbiRead32(u32 address)
    {
        auto& trace = s_ee_fastmem_abi_trace;
        ++trace.calls;
        trace.address = address;
        trace.pc = cpuRegs.pc;
        trace.delay_slot = cpuRegs.IsDelaySlot;
        trace.cycle_before = cpuRegs.cycle;
        cpuRegs.cycle += 7;
        cpuRegs.nextEventCycle = cpuRegs.cycle + 4096;
        trace.cycle_after = cpuRegs.cycle;
#if defined(__aarch64__)
        // Deliberately poison caller-saved SIMD registers at the fault helper.
        asm volatile(
            "movi v0.16b, #0xa5\n\t"
            "movi v1.16b, #0xa5\n\t"
            "movi v2.16b, #0xa5\n\t"
            "movi v3.16b, #0xa5\n\t"
            "movi v4.16b, #0xa5\n\t"
            "movi v5.16b, #0xa5\n\t"
            "movi v6.16b, #0xa5\n\t"
            "movi v7.16b, #0xa5\n\t"
            ::: "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "memory");
#endif
        return 0x89abcdefu ^ address;
    }

    struct EEFastmemAbiRestore
    {
        Pcsx2Config::RecompilerOptions config = EmuConfig.Cpu.Recompiler;
        R5900cpu* cpu = Cpu;
        vtlb_private::VTLBPhysical physical = vtlb_private::vtlbdata.pmap[EEFastmemAbiMmio >> 12];

        ~EEFastmemAbiRestore()
        {
            EmuCoreXOracleSetEeIrFastLoads(-1);
            vtlb_VMapUnmap(EEFastmemAbiAlias, EEFastmemAbiPage);
            if (physical.isHandler())
                vtlb_MapHandler(physical.assumeHandler(), EEFastmemAbiMmio, EEFastmemAbiPage);
            else
                vtlb_MapBlock(reinterpret_cast<void*>(physical.assumePtr()), EEFastmemAbiMmio, EEFastmemAbiPage);
            EmuConfig.Cpu.Recompiler = config;
            Cpu = cpu;
        }
    };

    struct EEFastmemAbiHooks
    {
        oak::Label* continuation = nullptr;
    };

    void EEFastmemAbiBeforeMemory(void*, u32 guest_pc, bool delay_slot)
    {
        oakAsm->MOV(oak::util::W16, guest_pc);
        oakStore32(oak::util::W16, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.pc))});
        oakAsm->MOV(oak::util::W16, delay_slot ? 1 : 0);
        oakStore32(oak::util::W16, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.IsDelaySlot))});
    }

    void EEFastmemAbiAfterMemory(void*)
    {
        oakAsm->MOV(oak::util::W16, 0);
        oakStore32(oak::util::W16, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.IsDelaySlot))});
    }

    void EEFastmemAbiBeforeHelper(void*)
    {
        oakLoad32(oak::util::W16, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.nextEventCycle))});
        oakAsm->ADD(oak::util::W24, oak::util::W24, oak::util::W16);
        oakStore32(oak::util::W24, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.cycle))});
    }

    void EEFastmemAbiAfterHelper(void*)
    {
        oakLoad32(oak::util::W16, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.cycle))});
        oakLoad32(oak::util::W17, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.nextEventCycle))});
        oakAsm->SUB(oak::util::W24, oak::util::W16, oak::util::W17);
    }

    void EEFastmemAbiExit(void* ctx, u32, bool)
    {
        // Inline bodies do not own LR. Flush the pinned cycle delta and branch
        // to the wrapper continuation; an MMIO dispatcher may have clobbered X30.
        oakLoad32(oak::util::W16, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.nextEventCycle))});
        oakAsm->ADD(oak::util::W24, oak::util::W24, oak::util::W16);
        oakStore32(oak::util::W24, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.cycle))});
        oakAsm->B(*static_cast<EEFastmemAbiHooks*>(ctx)->continuation);
    }

    ir::Function EEFastmemAbiFixture()
    {
        ir::Function fn;
        ir::Builder b(fn);
        b.CreateBlock(EEFastmemAbiPc);
        b.SetGuestPc(EEFastmemAbiPc);
        std::array<u32, 6> vectors{};
        for (u32 i = 0; i < vectors.size(); ++i)
            vectors[i] = b.Emit(ir::Op::ReadGpr, ir::Type::V4U32, {}, i + 1);
        std::array<u32, 5> scalars{};
        for (u32 i = 0; i < scalars.size(); ++i)
            scalars[i] = b.Emit(ir::Op::ReadGpr, ir::Type::I32, {}, i + 16);
        const u32 loaded = b.Emit1(ir::Op::Load32, ir::Type::I32, b.ConstI32(EEFastmemAbiAlias));
        for (u32 i = 0; i < vectors.size(); ++i)
            b.Emit1(ir::Op::WriteGpr, ir::Type::Void, vectors[i], i + 8);
        for (u32 i = 0; i < scalars.size(); ++i)
            b.Emit1(ir::Op::WriteGpr, ir::Type::Void, scalars[i], i + 22);
        b.Emit1(ir::Op::WriteGpr, ir::Type::Void, loaded, 28);
        b.Resume(EEFastmemAbiPc + 4);
        return fn;
    }

    void EEFastmemAbiTests()
    {
#if defined(__aarch64__) && defined(ARCH_ARM64) && defined(__ANDROID__)
        EEFastmemAbiRestore restore;
        Cpu = &recCpu;
        EmuCoreXOracleSetEeIrFastLoads(1);
        const vtlbHandler handler = vtlb_RegisterHandler(nullptr, nullptr, EEFastmemAbiRead32,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        vtlb_MapHandler(handler, EEFastmemAbiMmio, EEFastmemAbiPage);
        vtlb_VMap(EEFastmemAbiAlias, EEFastmemAbiMmio, EEFastmemAbiPage);

        std::memset(&cpuRegs, 0, sizeof(cpuRegs));
        for (u32 r = 1; r < 32; ++r)
        {
            cpuRegs.GPR.r[r].UD[0] = 0x1020304000000000ull | (0x13570000u + r * 0x101u);
            cpuRegs.GPR.r[r].UD[1] = 0x5060708000000000ull | (0x24680000u + r * 0x103u);
        }
        cpuRegs.cycle = 1000;
        cpuRegs.nextEventCycle = 900;
        cpuRegs.pc = EEFastmemAbiPc;
        cpuRegs.IsDelaySlot = 0;
        s_ee_fastmem_abi_trace = {};
        const _x86regs saved_rcx = x86regs[EE_HOST_RCX];
        const u32 saved_const = g_cpuHasConstReg;
        const u32 saved_flushed_const = g_cpuFlushedConstReg;
        ScopedGuard restore_allocator([&]() {
            x86regs[EE_HOST_RCX] = saved_rcx;
            g_cpuHasConstReg = saved_const;
            g_cpuFlushedConstReg = saved_flushed_const;
        });
        g_cpuHasConstReg &= ~(1u << 30);
        g_cpuFlushedConstReg &= ~(1u << 30);
        x86regs[EE_HOST_RCX] = {};
        x86regs[EE_HOST_RCX].inuse = 1;
        x86regs[EE_HOST_RCX].reg = 30;
        x86regs[EE_HOST_RCX].type = X86TYPE_GPR;
        x86regs[EE_HOST_RCX].mode = MODE_READ | MODE_WRITE;
        std::array<u8, sizeof(x86regs)> legacy_gpr_allocator_before{};
        std::array<u8, sizeof(xmmregs)> legacy_vector_allocator_before{};
        std::memcpy(legacy_gpr_allocator_before.data(), x86regs, sizeof(x86regs));
        std::memcpy(legacy_vector_allocator_before.data(), xmmregs, sizeof(xmmregs));

        ir::Function fn = EEFastmemAbiFixture();
        EEFastmemAbiHooks hook_context;
        oak::Label body;
        oak::Label continuation;
        hook_context.continuation = &continuation;
        EeIr::LowerHooks hooks{};
        hooks.guest_exit = EEFastmemAbiExit;
        hooks.before_memory = EEFastmemAbiBeforeMemory;
        hooks.after_memory = EEFastmemAbiAfterMemory;
        hooks.before_helper = EEFastmemAbiBeforeHelper;
        hooks.after_helper = EEFastmemAbiAfterHelper;
        hooks.ctx = &hook_context;
        hooks.ee_fastmem_abi = true;
        hooks.memory_preserves_vectors = true;
        EeIr::LowerOptions options{};
        options.inline_body = true;
        options.hooks = &hooks;
        options.guest_state = Arm64Ir::GuestState::EE;
        options.fastmem_ee_loads = true;
        options.direct_ee_loads = true;
        EmuConfig.Cpu.Recompiler.EnableEE = true;
        EmuConfig.Cpu.Recompiler.EnableEECache = false;
        EmuConfig.Cpu.Recompiler.EnableFastmem = true;
        EmuConfig.Cpu.Recompiler.PauseOnTLBMiss = false;

        u8* const entry = recBeginThunk();
        using namespace oak::util;
        // Save the dispatcher-preserved host state, including incoming LR.
        oakAsm->STP(X19, X20, SP, oak::PreIndexed{}, oak::SOffset<10, 3>(-16));
        oakAsm->STP(X21, X22, SP, oak::PreIndexed{}, oak::SOffset<10, 3>(-16));
        oakAsm->STP(X23, X24, SP, oak::PreIndexed{}, oak::SOffset<10, 3>(-16));
        oakAsm->STP(X25, X26, SP, oak::PreIndexed{}, oak::SOffset<10, 3>(-16));
        oakAsm->STP(X27, X28, SP, oak::PreIndexed{}, oak::SOffset<10, 3>(-16));
        oakAsm->STP(X29, X30, SP, oak::PreIndexed{}, oak::SOffset<10, 3>(-16));
        oakMoveAddressToReg(X27, &g_cpuRegistersPack);
        oakLoad64(X19, {X27, static_cast<s64>(offsetof(cpuRegistersPack, vtlbdata.fastmem_base))});
        oakLoad32(W16, {X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.cycle))});
        oakLoad32(W17, {X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.nextEventCycle))});
        oakAsm->SUB(W24, W16, W17);
        oakAsm->B(body);
        oakAsm->l(body);

        EeIr::LowerOutput out{};
        std::string error;
        const bool lowered = EeIr::LowerBlock(fn, options, nullptr, 0, &out, &error);
        oakAsm->l(continuation);
        oakAsm->LDP(X29, X30, SP, oak::PostIndexed{}, oak::SOffset<10, 3>(16));
        oakAsm->LDP(X27, X28, SP, oak::PostIndexed{}, oak::SOffset<10, 3>(16));
        oakAsm->LDP(X25, X26, SP, oak::PostIndexed{}, oak::SOffset<10, 3>(16));
        oakAsm->LDP(X23, X24, SP, oak::PostIndexed{}, oak::SOffset<10, 3>(16));
        oakAsm->LDP(X21, X22, SP, oak::PostIndexed{}, oak::SOffset<10, 3>(16));
        oakAsm->LDP(X19, X20, SP, oak::PostIndexed{}, oak::SOffset<10, 3>(16));
        oakAsm->RET();
        recEndThunk();

        if (lowered)
            reinterpret_cast<void (*)()>(entry)();
        bool state_ok = true;
        for (u32 i = 0; i < 6; ++i)
        {
            const u32 source = i + 1;
            const u32 destination = i + 8;
            const u64 expected_low = 0x1020304000000000ull | (0x13570000u + source * 0x101u);
            const u64 expected_high = 0x5060708000000000ull | (0x24680000u + source * 0x103u);
            state_ok &= cpuRegs.GPR.r[destination].UD[0] == expected_low &&
                cpuRegs.GPR.r[destination].UD[1] == expected_high;
        }
        for (u32 i = 0; i < 5; ++i)
        {
            const u32 expected = 0x13570000u + (i + 16) * 0x101u;
            state_ok &= cpuRegs.GPR.r[i + 22].UL[0] == expected &&
                cpuRegs.GPR.r[i + 22].UL[1] == ((expected & 0x80000000u) ? 0xffffffffu : 0u);
        }
        const u32 expected_load = 0x89abcdefu ^ EEFastmemAbiMmio;
        Check(lowered && out.fastmem_ee_load_operations == 1,
            "EE ABI inline frame registers one real fastmem load site");
        Check(out.frame_size > 0 && out.spill_values > 0 && out.vector_register_values >= 6 && out.vector_save_values >= 6,
            "EE ABI inline frame keeps six full-width Q values and X21-X23 scalar values live across load");
        Check(state_ok && cpuRegs.GPR.r[28].UD[0] == static_cast<u64>(static_cast<s64>(static_cast<s32>(expected_load))),
            "EE ABI fault helper preserves live Q and scalar values and returns loaded word");
        Check(s_ee_fastmem_abi_trace.calls == 1 && s_ee_fastmem_abi_trace.address == EEFastmemAbiMmio &&
            s_ee_fastmem_abi_trace.pc == EEFastmemAbiPc && s_ee_fastmem_abi_trace.delay_slot == 0 &&
            s_ee_fastmem_abi_trace.cycle_before == 1000 &&
            s_ee_fastmem_abi_trace.cycle_after == 1007 &&
            cpuRegs.cycle == 1007 && cpuRegs.nextEventCycle == 5103,
            "EE ABI MMIO fallback retains guest fault metadata and synchronizes W24 cycle delta");
        Check(vtlb_IsFaultingPC(EEFastmemAbiPc), "EE ABI load backpatch marks its guest fault PC");
        Check(std::memcmp(legacy_gpr_allocator_before.data(), x86regs, sizeof(x86regs)) == 0 &&
            std::memcmp(legacy_vector_allocator_before.data(), xmmregs, sizeof(xmmregs)) == 0,
            "EE ABI lowering and fastmem backpatch leave legacy allocator metadata unchanged");

        EmuCoreXOracleSetEeIrFastLoads(-1);
#endif
    }
}

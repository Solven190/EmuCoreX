// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+
//
// VABS bound common-IR oracle tests. Included inside jit_oracle.cpp's anonymous namespace.
#include "arm64/ir/IrLower-arm64.h"

extern "C" u64 EmuCoreXOracleVUIRAbsInstructions(u32 index);

void VUSharedVabsIntegrationTests();
void VUSharedVabsBenchmarks();

ir::Function BoundVfUnaryFixture(u32 src, u32 dst, u32 mask, bool delay_slot = false)
{
    ir::Function fn;
    ir::Builder b(fn);
    b.CreateBlock(0x80);
    b.SetDelaySlot(delay_slot);
    const u32 source = b.Emit(ir::Op::ReadVf, ir::Type::V4F32, {}, src);
    const u32 absolute = b.Emit1(ir::Op::VAbs, ir::Type::V4F32, source);
    b.Emit1(ir::Op::WriteVfMasked, ir::Type::Void, absolute, dst,
        static_cast<u16>(mask | (delay_slot ? ir::IF_DELAY_SLOT : 0)));
    b.Return();
    return fn;
}

void VUBoundVfUnaryTests()
{
    const std::array<std::array<u32, 4>, 4> sources = {{
        {{0x00000000u, 0x80000000u, 0x7f800000u, 0xff800000u}},
        {{0x7fc12345u, 0xffc54321u, 0x7fa00001u, 0xffa12345u}},
        {{0x00000001u, 0x80000001u, 0x007fffffu, 0x807fffffu}},
        {{0xffffffffu, 0x12345678u, 0xdeadbeefu, 0x00000000u}},
    }};
    const std::array<u32, 4> initial = {0x13579bdfu, 0x2468ace0u, 0xdeadbeefu, 0x01234567u};

    bool all_masks_accepted = true;
    std::array<Arm64Ir::BoundVfUnaryPlan, 15> plans{};
    for (u32 mask = 1; mask < 16; ++mask)
    {
        const ir::Function fn = BoundVfUnaryFixture(7, 9, mask);
        std::string error;
        Arm64Ir::BoundVfUnaryPlan plan;
        const bool accepted = ir::Verify(fn, &error) && Arm64Ir::PlanBoundVfUnary(fn, &plan, &error);
        all_masks_accepted &= accepted && plan.src_vf == 7 && plan.dst_vf == 9 &&
            plan.lane_mask == mask && !plan.delay_slot;
        if (accepted)
            plans[mask - 1] = plan;
    }
    Check(all_masks_accepted, "shared VU VABS preflight accepts all 15 lane masks");

    if (all_masks_accepted)
    {
        bool all_raw_bits_correct = true;
        for (u32 mask = 1; mask < 16; ++mask)
        for (const auto& source : sources)
        for (u32 alias = 0; alias < 2; ++alias)
        for (Arm64Ir::BoundVfUnaryLayout layout : {Arm64Ir::BoundVfUnaryLayout::GuestLanes,
            Arm64Ir::BoundVfUnaryLayout::MicroVuPartial})
        {
            std::array<u32, 4> actual{};
            oakSetAsmPtr(SysMemory::GetEERec() + 0x2000000, 0x10000);
            u8* entry = oakStartBlock();
            // The lowerer receives the full raw VF source, even for one-lane
            // MicroVuPartial writes; it performs any S0 packing itself.
            if (alias)
                oakAsm->LDR(oak::QReg(4), oak::util::X0);
            else
            {
                oakAsm->LDR(oak::QReg(3), oak::util::X0);
                oakAsm->LDR(oak::QReg(4), oak::util::X1);
            }
            const u8* before = oakGetCurrentCodePointer();
            Arm64Ir::LowerOutput out;
            std::string error;
            const bool lowered = Arm64Ir::LowerBoundVfUnaryPlan(plans[mask - 1], alias ? 4 : 3, 4,
                &out, &error, layout);
            const size_t emitted = static_cast<size_t>(oakGetCurrentCodePointer() - before);
            oakAsm->STR(oak::QReg(4), oak::util::X2);
            oakAsm->RET();
            oakEndBlock();
            reinterpret_cast<void (*)(const u32*, const u32*, u32*)>(entry)(
                source.data(), alias ? source.data() : initial.data(), actual.data());
            const bool one_lane = mask == 1 || mask == 2 || mask == 4 || mask == 8;
            for (u32 lane = 0; lane < 4; ++lane)
            {
                if (layout == Arm64Ir::BoundVfUnaryLayout::GuestLanes)
                {
                    const u32 expected = (mask & (8u >> lane)) ? source[lane] & 0x7fffffffu :
                        (alias ? source[lane] : initial[lane]);
                    all_raw_bits_correct &= actual[lane] == expected;
                }
                else if (mask & (8u >> lane))
                {
                    const u32 output_lane = one_lane ? 0 : lane;
                    all_raw_bits_correct &= actual[output_lane] == (source[lane] & 0x7fffffffu);
                }
            }
            all_raw_bits_correct &= lowered && emitted != 0 && out.host_size == emitted &&
                out.frame_size == 0 && out.spill_values == 0;
        }
        Check(all_raw_bits_correct,
            "shared VU VABS clears only selected raw sign bits across masks, layouts, and aliases");
    }

    // VF0 is a legal immutable source; VF0 cannot be a writable destination.
    {
        std::string error;
        Arm64Ir::BoundVfUnaryPlan plan;
        const bool source_zero = Arm64Ir::PlanBoundVfUnary(BoundVfUnaryFixture(0, 9, 15), &plan, &error) &&
            plan.src_vf == 0 && plan.dst_vf == 9;
        const bool destination_zero = Arm64Ir::PlanBoundVfUnary(BoundVfUnaryFixture(7, 0, 15), &plan, &error);
        Check(source_zero && !destination_zero, "shared VU VABS accepts VF0 source and rejects VF0 destination");
    }

    // Delay metadata must be carried by the whole fixed slice.
    {
        std::string error;
        Arm64Ir::BoundVfUnaryPlan plan;
        const bool valid_delay = Arm64Ir::PlanBoundVfUnary(BoundVfUnaryFixture(7, 9, 5, true), &plan, &error) &&
            plan.delay_slot;
        Check(valid_delay, "shared VU VABS preserves consistent branch-delay metadata");
    }

    auto valid_fn = BoundVfUnaryFixture(7, 9, 15);
    Arm64Ir::BoundVfUnaryPlan valid_plan;
    std::string error;
    const bool valid_plan_ok = Arm64Ir::PlanBoundVfUnary(valid_fn, &valid_plan, &error);
    bool malformed_rejected = true;
    const auto reject = [&](ir::Function fn) {
        Arm64Ir::BoundVfUnaryPlan plan{1, 2, 3, true};
        const bool accepted = Arm64Ir::PlanBoundVfUnary(fn, &plan, &error);
        malformed_rejected &= !accepted && plan.src_vf == 0 && plan.dst_vf == 0 &&
            plan.lane_mask == 0 && !plan.delay_slot;
    };
    for (u32 bad_mask : {0u, 16u}) reject(BoundVfUnaryFixture(7, 9, bad_mask));
    reject(BoundVfUnaryFixture(32, 9, 15));
    reject(BoundVfUnaryFixture(7, 32, 15));
    {
        auto fn = valid_fn;
        fn.blocks[0].insts[1].op = ir::Op::VNeg;
        reject(std::move(fn));
    }
    {
        auto fn = valid_fn;
        fn.blocks[0].insts[1].type = ir::Type::V4U32;
        fn.value_types[fn.blocks[0].insts[1].value] = ir::Type::V4U32;
        reject(std::move(fn));
    }
    {
        auto fn = valid_fn;
        fn.blocks[0].insts[2].args[0] = fn.blocks[0].insts[0].value;
        reject(std::move(fn));
    }
    {
        auto fn = valid_fn;
        fn.blocks[0].insts[1].aux = ir::IF_EXACT;
        reject(std::move(fn));
    }
    {
        auto fn = valid_fn;
        fn.blocks[0].insts[1].imm = 1;
        reject(std::move(fn));
    }
    {
        auto fn = valid_fn;
        fn.blocks[0].insts[0].aux = ir::IF_DELAY_SLOT;
        reject(std::move(fn));
    }
    {
        auto fn = valid_fn;
        ir::Inst extra;
        extra.op = ir::Op::Nop;
        fn.blocks[0].insts.insert(fn.blocks[0].insts.end() - 1, extra);
        reject(std::move(fn));
    }
    {
        auto fn = valid_fn;
        ir::Builder b(fn);
        b.CreateBlock(0x90);
        b.Return();
        reject(std::move(fn));
    }
    Check(malformed_rejected, "shared VU VABS clears and rejects malformed masks, indices, types, links, flags, and shapes");

    // Every invalid lowering call must leave the active buffer untouched.
    if (valid_plan_ok)
    {
        for (const std::array<u32, 2> binding : {std::array<u32, 2>{31, 4}, {3, 31},
            {32, 4}, {static_cast<u32>(OAK_QSCRATCH.index()), 4}, {3, static_cast<u32>(OAK_QSCRATCH.index())}})
        {
            oakSetAsmPtr(SysMemory::GetEERec() + 0x2000000, 0x10000);
            oakStartBlock();
            const u8* before = oakGetCurrentCodePointer();
            Arm64Ir::LowerOutput out{reinterpret_cast<u8*>(1), 1, 1};
            const bool lowered = Arm64Ir::LowerBoundVfUnaryPlan(valid_plan, binding[0], binding[1], &out, &error,
                Arm64Ir::BoundVfUnaryLayout::MicroVuPartial);
            const bool no_code = before == oakGetCurrentCodePointer() && out.host_size == 0 &&
                out.frame_size == 0 && out.entry == nullptr;
            oakAsm->RET();
            oakEndBlock();
            Check(!lowered && no_code, "shared VU VABS rejects invalid host bindings without emission");
        }

        oakSetAsmPtr(SysMemory::GetEERec() + 0x2000000, 0x10000);
        oakStartBlock();
        const u8* before = oakGetCurrentCodePointer();
        Arm64Ir::LowerOutput out;
        const bool lowered = Arm64Ir::LowerBoundVfUnaryPlan(valid_plan, 3, 4, &out, &error,
            static_cast<Arm64Ir::BoundVfUnaryLayout>(2));
        const bool no_code = before == oakGetCurrentCodePointer() && out.host_size == 0;
        oakAsm->RET();
        oakEndBlock();
        Check(!lowered && no_code, "shared VU VABS rejects unknown destination layout without emission");

        for (const Arm64Ir::BoundVfUnaryPlan bad_plan : {
            Arm64Ir::BoundVfUnaryPlan{32, 9, 15, false},
            Arm64Ir::BoundVfUnaryPlan{7, 0, 15, false},
            Arm64Ir::BoundVfUnaryPlan{7, 9, 0, false},
            Arm64Ir::BoundVfUnaryPlan{7, 9, 16, false}})
        {
            oakSetAsmPtr(SysMemory::GetEERec() + 0x2000000, 0x10000);
            oakStartBlock();
            const u8* plan_before = oakGetCurrentCodePointer();
            Arm64Ir::LowerOutput bad_out{reinterpret_cast<u8*>(1), 1, 1};
            const bool bad_lowered = Arm64Ir::LowerBoundVfUnaryPlan(bad_plan, 3, 4, &bad_out, &error,
                Arm64Ir::BoundVfUnaryLayout::MicroVuPartial);
            const bool bad_no_code = plan_before == oakGetCurrentCodePointer() && bad_out.host_size == 0 &&
                bad_out.frame_size == 0 && bad_out.entry == nullptr;
            oakAsm->RET();
            oakEndBlock();
            Check(!bad_lowered && bad_no_code, "shared VU VABS rejects malformed plans without emission");
        }
    }

    bool counters_zero = true;
    for (u32 vu = 0; vu < 2; ++vu)
        counters_zero &= EmuCoreXOracleVUIRAbsInstructions(vu) == 0;
    Check(counters_zero, "shared VU VABS production counters remain zero before upper routing");

    VUSharedVabsIntegrationTests();
    VUSharedVabsBenchmarks();
}

void VUSharedVabsIntegrationTests()
{
    const auto vabs = [](u32 fs, u32 ft, u32 mask)
        { return 0x3du | (7u << 6) | (fs << 11) | (ft << 16) | ((mask & 15u) << 21); };
    const auto mr32 = [](u32 fs, u32 ft, u32 mask)
        { return 0x8000033du | (fs << 11) | (ft << 16) | (mask << 21); };
    const auto move = [](u32 fs, u32 ft, u32 mask)
        { return 0x8000033cu | (fs << 11) | (ft << 16) | ((mask & 15u) << 21); };
    // Verify the field layout used by the production upper decoder: major
    // index 0x3d selects FD_01, whose subopcode 7 is ABS.
    const u32 encoding_probe = vabs(7, 9, 5);
    Check((encoding_probe & 0x3fu) == 0x3du && ((encoding_probe >> 6) & 0x1fu) == 7u &&
        ((encoding_probe >> 11) & 0x1fu) == 7u && ((encoding_probe >> 16) & 0x1fu) == 9u &&
        ((encoding_probe >> 21) & 0x0fu) == 5u,
        "VABS fixture encodes upper major 0x3d and FD_01 subopcode 7");
    std::array<u32, 128> vf{};
    vf[3] = 0x3f800000u;
    for (u32 r = 1; r < 32; ++r)
        for (u32 lane = 0; lane < 4; ++lane)
            vf[r * 4 + lane] = (r * 0x91e10da5u) ^ (lane * 0x80808080u) ^ 0xffc12345u;

    for (u32 vu = 0; vu < 2; ++vu)
    {
        for (u32 mask = 1; mask < 16; ++mask)
        {
            const std::vector<u32> program = {NOP_L, vabs(7, 9, mask), NOP_L,
                NOP_U | 0x40000000u, NOP_L, NOP_U};
            EmuCoreXOracleSetVUIR(vu, 0);
            const u64 legacy_count = EmuCoreXOracleVUIRAbsInstructions(vu);
            const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, vf.data());
            const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, vf.data());
            Check(EmuCoreXOracleVUIRAbsInstructions(vu) == legacy_count,
                "legacy upper VABS does not increment shared-IR counter");
            EmuCoreXOracleSetVUIR(vu, 1);
            const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, vf.data());
            Compare(interp, shared, "shared upper VABS matches interpreter for every lane mask");
            Compare(legacy, shared, "shared upper VABS matches legacy JIT state and memory");
            std::array<u32, 128> expected_vf{};
            std::memcpy(expected_vf.data(), vf.data(), sizeof(expected_vf));
            for (u32 lane = 0; lane < 4; ++lane)
                if (mask & (8u >> lane))
                    expected_vf[9 * 4 + lane] = expected_vf[7 * 4 + lane] & 0x7fffffffu;
            const bool expected_bits = std::memcmp(interp.regs.VF, expected_vf.data(), sizeof(expected_vf)) == 0 &&
                std::memcmp(legacy.regs.VF, expected_vf.data(), sizeof(expected_vf)) == 0 &&
                std::memcmp(shared.regs.VF, expected_vf.data(), sizeof(expected_vf)) == 0;
            Check(expected_bits, "interpreter, legacy, and shared VABS match independent raw-bit VF expectation");
            Check(shared.regs.cycle == legacy.regs.cycle,
                "shared upper VABS preserves exact cycle count");
            // RunVU currently initializes the internal flag pipeline to zero;
            // these are zero-state preservation comparisons, not a seeded-flag proof.
            Check(shared.regs.macflag == legacy.regs.macflag &&
                shared.regs.statusflag == legacy.regs.statusflag &&
                shared.regs.clipflag == legacy.regs.clipflag,
                "shared upper VABS preserves MAC, STATUS, and CLIP flags");
            Check(EmuCoreXOracleVUIRAbsInstructions(vu) == legacy_count + 1,
                "eligible upper VABS routes through common IR on both VU cores");
        }

        // Exercise allocator source-before-destination behavior for Fs==Ft in
        // the real microVU integration, with a full and a partial write mask.
        for (u32 mask : {15u, 4u})
        {
            const std::vector<u32> program = {NOP_L, vabs(7, 7, mask), NOP_L,
                NOP_U | 0x40000000u, NOP_L, NOP_U};
            EmuCoreXOracleSetVUIR(vu, 0);
            const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, vf.data());
            const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, vf.data());
            const u64 before = EmuCoreXOracleVUIRAbsInstructions(vu);
            EmuCoreXOracleSetVUIR(vu, 1);
            const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, vf.data());
            Compare(interp, shared, "shared production VABS preserves source/destination aliasing");
            Compare(legacy, shared, "aliased shared VABS matches legacy JIT");
            Check(shared.regs.cycle == legacy.regs.cycle,
                "aliased production VABS preserves exact cycles");
            std::array<u32, 128> expected_vf{};
            std::memcpy(expected_vf.data(), vf.data(), sizeof(expected_vf));
            for (u32 lane = 0; lane < 4; ++lane)
                if (mask & (8u >> lane))
                    expected_vf[7 * 4 + lane] &= 0x7fffffffu;
            Check(std::memcmp(shared.regs.VF, expected_vf.data(), sizeof(expected_vf)) == 0,
                "aliased production VABS matches independent raw-bit expectation");
            Check(EmuCoreXOracleVUIRAbsInstructions(vu) == before + 1,
                "aliased production VABS routes through common IR");
        }

        // A zero lane mask is skipped by the existing scheduler and must not
        // be counted as a shared-IR compile.
        {
            const std::vector<u32> program = {NOP_L, vabs(7, 9, 0), NOP_L,
                NOP_U | 0x40000000u, NOP_L, NOP_U};
            EmuCoreXOracleSetVUIR(vu, 0);
            const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, vf.data());
            const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, vf.data());
            const u64 before = EmuCoreXOracleVUIRAbsInstructions(vu);
            EmuCoreXOracleSetVUIR(vu, 1);
            const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, vf.data());
            Compare(interp, legacy, "zero-mask VABS interpreter/legacy full-state no-op");
            Compare(interp, shared, "zero-mask VABS shared-enabled full-state no-op");
            Check(shared.regs.cycle == legacy.regs.cycle &&
                EmuCoreXOracleVUIRAbsInstructions(vu) == before,
                "zero-mask VABS preserves cycles and skips shared compilation");
        }

        // VF0 is the architectural +1 vector and remains a valid source.
        {
            const std::vector<u32> program = {NOP_L, vabs(0, 9, 15), NOP_L,
                NOP_U | 0x40000000u, NOP_L, NOP_U};
            EmuCoreXOracleSetVUIR(vu, 0);
            const Snapshot legacy = RunVU(vu, true, program, 0x80);
            const u64 before = EmuCoreXOracleVUIRAbsInstructions(vu);
            EmuCoreXOracleSetVUIR(vu, 1);
            const Snapshot shared = RunVU(vu, true, program, 0x80);
            Compare(legacy, shared, "shared upper VABS accepts immutable VF0 source");
            Check(EmuCoreXOracleVUIRAbsInstructions(vu) == before + 1,
                "VF0 source VABS is counted as eligible shared IR");
        }

        // VF0 destinations and disabled shared-IR mode must stay on legacy code.
        {
            const std::vector<u32> program = {NOP_L, vabs(7, 0, 15), NOP_L,
                NOP_U | 0x40000000u, NOP_L, NOP_U};
            const u64 before = EmuCoreXOracleVUIRAbsInstructions(vu);
            EmuCoreXOracleSetVUIR(vu, 1);
            RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, vf.data());
            Check(EmuCoreXOracleVUIRAbsInstructions(vu) == before,
                "VF0 destination VABS remains legacy no-op");
            EmuCoreXOracleSetVUIR(vu, 0);
            RunVU(vu, true, {NOP_L, vabs(7, 9, 15), NOP_L, NOP_U | 0x40000000u, NOP_L, NOP_U},
                0x80, 0, 128, false, nullptr, vf.data());
            Check(EmuCoreXOracleVUIRAbsInstructions(vu) == before,
                "disabled shared-IR mode does not increment VABS counter");
        }

        // The upper VABS wins a same-pair WAW against lower MR32, as before.
        {
            const std::vector<u32> program = {mr32(6, 9, 15), vabs(7, 9, 5), NOP_L,
                NOP_U | 0x40000000u, NOP_L, NOP_U};
            EmuCoreXOracleSetVUIR(vu, 0);
            const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, vf.data());
            const u64 before = EmuCoreXOracleVUIRAbsInstructions(vu);
            EmuCoreXOracleSetVUIR(vu, 1);
            const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, vf.data());
            Compare(legacy, shared, "shared upper VABS preserves same-pair lower/upper write priority");
            Check(EmuCoreXOracleVUIRAbsInstructions(vu) == before + 1,
                "same-pair priority case still routes upper VABS through common IR");
        }

        // Exercise an actual branch delay slot and retain all flag/cycle state.
        {
            const u32 branch = (0x28u << 25) | (1u << 11) | (2u << 16) | 2u;
            const std::vector<u32> program = {branch, NOP_U, NOP_L, vabs(7, 9, 10),
                NOP_L, NOP_U, NOP_L, NOP_U | 0x40000000u, NOP_L, NOP_U};
            std::array<u32, 16> vi{};
            vi[1] = 0;
            vi[2] = 0;
            EmuCoreXOracleSetVUIR(vu, 0);
            const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false,
                nullptr, vf.data(), nullptr, vi.data());
            const u64 before = EmuCoreXOracleVUIRAbsInstructions(vu);
            EmuCoreXOracleSetVUIR(vu, 1);
            const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false,
                nullptr, vf.data(), nullptr, vi.data());
            Compare(legacy, shared, "shared upper VABS in branch delay slot preserves legacy state");
            Check(EmuCoreXOracleVUIRAbsInstructions(vu) == before + 1,
                "branch-delay upper VABS routes through common IR");
        }

        // A lower MOVE producer immediately feeds the next pair's ABS source.
        {
            const std::vector<u32> program = {move(5, 7, 15), NOP_U,
                NOP_L, vabs(7, 9, 15), NOP_L, NOP_U | 0x40000000u, NOP_L, NOP_U};
            EmuCoreXOracleSetVUIR(vu, 0);
            const Snapshot legacy = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, vf.data());
            const u64 before = EmuCoreXOracleVUIRAbsInstructions(vu);
            EmuCoreXOracleSetVUIR(vu, 1);
            const Snapshot shared = RunVU(vu, true, program, 0x80, 0, 128, false, nullptr, vf.data());
            Compare(legacy, shared, "shared upper VABS observes preceding lower MOVE producer");
            std::array<u32, 128> expected_vf{};
            std::memcpy(expected_vf.data(), vf.data(), sizeof(expected_vf));
            for (u32 lane = 0; lane < 4; ++lane)
                expected_vf[7 * 4 + lane] = expected_vf[5 * 4 + lane];
            for (u32 lane = 0; lane < 4; ++lane)
                expected_vf[9 * 4 + lane] = expected_vf[7 * 4 + lane] & 0x7fffffffu;
            const Snapshot interp = RunVU(vu, false, program, 0x80, 0, 128, false, nullptr, vf.data());
            const bool expected_bits = std::memcmp(interp.regs.VF, expected_vf.data(), sizeof(expected_vf)) == 0 &&
                std::memcmp(legacy.regs.VF, expected_vf.data(), sizeof(expected_vf)) == 0 &&
                std::memcmp(shared.regs.VF, expected_vf.data(), sizeof(expected_vf)) == 0;
            Check(expected_bits, "lower MOVE to upper VABS matches independent sequential VF simulation");
            Check(EmuCoreXOracleVUIRAbsInstructions(vu) == before + 1,
                "lower MOVE dependency still routes the upper VABS through common IR");
        }
        EmuCoreXOracleSetVUIR(vu, -1);
    }
}

void VUSharedVabsBenchmarks()
{
    const FPControlRegisterBackup callerFPCR(EmuConfig.Cpu.FPUFPCR);
    const auto vabs = [](u32 fs, u32 ft, u32 mask)
        { return 0x3du | (7u << 6) | (fs << 11) | (ft << 16) | ((mask & 15u) << 21); };
    const auto iadd = [](u32 is, u32 it, u32 id)
        { return 0x80000000u | 0x30u | (is << 11) | (it << 16) | (id << 6); };
    std::array<u32, 16> vi{};
    std::array<u32, 128> vf{};
    vf[3] = 0x3f800000u;
    for (u32 r = 1; r < 32; ++r)
        for (u32 lane = 0; lane < 4; ++lane)
            vf[r * 4 + lane] = (r * 0x9e3779b9u) ^ (lane * 0x80000000u) ^ 0xffa12345u;

    for (u32 vu = 0; vu < 2; ++vu)
    {
        std::vector<u32> program;
        for (u32 step = 0; step < 24; ++step)
            program.insert(program.end(), {iadd(1, 2, 3 + step % 10),
                vabs(1 + step % 31, 1 + (step + 7) % 31, 1 + step % 15)});
        program.insert(program.end(), {NOP_L, NOP_U | 0x40000000u, NOP_L, NOP_U});
        std::array<u32, 128> expected_vf{};
        std::memcpy(expected_vf.data(), vf.data(), sizeof(expected_vf));
        for (u32 step = 0; step < 24; ++step)
        {
            const u32 src = 1 + step % 31;
            const u32 dst = 1 + (step + 7) % 31;
            const u32 mask = 1 + step % 15;
            for (u32 lane = 0; lane < 4; ++lane)
                if (mask & (8u >> lane))
                    expected_vf[dst * 4 + lane] = expected_vf[src * 4 + lane] & 0x7fffffffu;
        }
        std::array<Snapshot, 2> expected;
        std::array<u64, 2> native_bytes{};
        std::array<double, 2> cold_us{};
        std::array<double, 2> hot_median_ns{};
        for (u32 mode = 0; mode < 2; ++mode)
        {
            EmuCoreXOracleSetVUIR(vu, mode ? 1 : 0);
            std::array<double, 7> cold_samples{};
            bool exact_cold_counts = true;
            for (u32 sample = 0; sample < cold_samples.size(); ++sample)
            {
                const u64 before = EmuCoreXOracleVUIRAbsInstructions(vu);
                const auto cold_start = std::chrono::steady_clock::now();
                expected[mode] = RunVU(vu, true, program, 0x80, 0, 128, false,
                    nullptr, vf.data(), nullptr, vi.data());
                cold_samples[sample] = std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - cold_start).count();
                const u64 after = EmuCoreXOracleVUIRAbsInstructions(vu);
                exact_cold_counts &= after - before == (mode ? 24u : 0u);
            }
            std::sort(cold_samples.begin(), cold_samples.end());
            cold_us[mode] = cold_samples[cold_samples.size() / 2];
            Check(exact_cold_counts, "VABS benchmark has exact ABS compile deltas on every fresh cold sample");
            Check(std::memcmp(expected[mode].regs.VF, expected_vf.data(), sizeof(expected_vf)) == 0,
                "VABS mixed-workload final VF matches independent raw-bit simulation");
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
                "VABS benchmark reports native bytes from the bounded fresh cache");

            VURegs& regs = vu ? VU1 : VU0;
            BaseVUmicroCPU* cpu = vu ? static_cast<BaseVUmicroCPU*>(&CpuMicroVU1) :
                static_cast<BaseVUmicroCPU*>(&CpuMicroVU0);
            const u64 before_hot = EmuCoreXOracleVUIRAbsInstructions(vu);
            const u64 before_hot_ir = EmuCoreXOracleVUIRInstructions(vu);
            std::array<double, 9> samples{};
            bool same = true;
            for (u32 sample = 0; sample < samples.size(); ++sample)
            {
                constexpr u32 iterations = 4096;
                const auto hot_start = std::chrono::steady_clock::now();
                for (u32 iteration = 0; iteration < iterations; ++iteration)
                {
                    std::memcpy(regs.VF, vf.data(), sizeof(regs.VF));
                    regs.cycle = 0;
                    regs.VI[REG_TPC].UL = 0x80 / 8;
                    VU0.VI[REG_VPU_STAT].UL = vu ? 0x100 : 1;
                    cpu->SetStartPC(0x80);
                    for (u32 call = 0; call < 128 && (VU0.VI[REG_VPU_STAT].UL & (vu ? 0x100 : 1)); ++call)
                        cpu->Execute(128);
                    same &= VU0.VI[REG_VPU_STAT].UL == 0;
                }
                samples[sample] = std::chrono::duration<double, std::nano>(
                    std::chrono::steady_clock::now() - hot_start).count() / iterations;
                same &= std::memcmp(regs.VF, expected[mode].regs.VF, sizeof(regs.VF)) == 0 &&
                    regs.cycle == expected[mode].regs.cycle;
            }
            std::sort(samples.begin(), samples.end());
            hot_median_ns[mode] = samples[samples.size() / 2];
            Check(same, "VABS hot benchmark replays cached code with expected VF state and cycles");
            Check(EmuCoreXOracleVUIRAbsInstructions(vu) == before_hot,
                "VABS hot benchmark performs no new ABS compiles");
            Check(EmuCoreXOracleVUIRInstructions(vu) == before_hot_ir,
                "VABS hot benchmark performs no new shared-IR compiles");
        }
        Compare(expected[0], expected[1], "VABS benchmark shared/legacy state comparison");
        Check(expected[0].regs.cycle == expected[1].regs.cycle,
            "VABS mixed benchmark preserves exact legacy/shared cycles");
        std::printf("VUIRABSBENCH core=%u vabs_ops=24 mixed_iadd_ops=24 cold_samples=7 hot_samples=9 hot_iterations=4096 legacy_fresh_reset_compile_median_us=%.3f shared_fresh_reset_compile_median_us=%.3f legacy_hot_median_ns_per_replay=%.3f shared_hot_median_ns_per_replay=%.3f legacy_native_block_bytes=%llu shared_native_block_bytes=%llu\n",
            vu, cold_us[0], cold_us[1], hot_median_ns[0], hot_median_ns[1],
            static_cast<unsigned long long>(native_bytes[0]),
            static_cast<unsigned long long>(native_bytes[1]));
    }
    EmuCoreXOracleSetVUIR(0, -1);
    EmuCoreXOracleSetVUIR(1, -1);
}

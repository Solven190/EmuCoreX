// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+
// Shared-IR EE COP1 condition-branch coverage.
// Included from jit_oracle.cpp's anonymous namespace.
#include "arm64/ee/EeIrLifter-arm64.h"

extern "C" u64 EmuCoreXOracleGetEeIrBc1Counts();

namespace
{
constexpr u32 EE_BC1_FLAG = 0x00800000u;

std::vector<u32> EEBc1ControlProgram(u32 branch_word, u32 delay_word, u32 branch_index = 0)
{
    std::vector<u32> code;
    if (branch_index != 0)
        code.push_back(MipsCop1(6, 8, 31, 0, 0)); // ctc1 t0, fcr31
    code.push_back(branch_word);
    code.push_back(delay_word);
    code.push_back(MipsI(9, 0, 9, 10));
    code.push_back(MipsJ(2, EE_TEST_PC + (branch_index + 8) * 4));
    code.push_back(0);
    code.push_back(MipsI(9, 0, 9, 20));
    code.push_back(MipsJ(2, EE_TEST_PC + (branch_index + 8) * 4));
    code.push_back(0);
    code.push_back(MipsJ(2, EE_TEST_PC + (branch_index + 8) * 4));
    code.push_back(0);
    return code;
}

void EEIrCop1BranchTests()
{
    const u32 fcr_mask = EE_BC1_FLAG;
    const u32 branch_forms[] = {0, 1, 2, 3}; // BC1F/T/FL/TL; Rt condition-code aliases are reserved.
    for (u32 rt : branch_forms)
    {
        const u32 branch = MipsI(0x11, 8, rt, 4);
        const std::vector<u32> lift_code = {branch, MipsI(9, 0, 10, 1), 0};
        ir::Function lifted;
        u32 end_pc = 0;
        std::string error;
        const bool accepted = EeIr::LiftBlock(lift_code.data(), {EE_TEST_PC, 3}, lifted, &end_pc, &error);
        char name[96];
        std::snprintf(name, sizeof(name), "ir BC1 form %u admitted by shared lifter", rt);
        Check(accepted, name); // Intentional RED until the BC1 branch planner is implemented.

        for (bool condition : {false, true})
        {
            u32 initial_fcr[32]{};
            if (condition)
                initial_fcr[31] = fcr_mask;
            const std::vector<u32> code = EEBc1ControlProgram(branch, MipsI(9, 0, 10, 1));
            const EESnapshot interp = RunEEProgram(false, code, true, nullptr, nullptr, nullptr, nullptr, initial_fcr);
            const EESnapshot legacy = RunEEProgramWithIrMode(0, code, nullptr, nullptr, nullptr, initial_fcr, true);
            const u64 before_shared = EmuCoreXOracleGetEeIrBc1Counts();
            const EESnapshot shared = RunEEProgramWithIrMode(1, code, nullptr, nullptr, nullptr, initial_fcr, true);
            Check(EmuCoreXOracleGetEeIrBc1Counts() > before_shared,
                "BC1 shared production compile counter proves route admission");
            std::snprintf(name, sizeof(name), "BC1 form %u condition %u interpreter/legacy", rt, condition);
            CompareEE(interp, legacy, name);
            std::snprintf(name, sizeof(name), "BC1 form %u condition %u interpreter/shared", rt, condition);
            CompareEE(interp, shared, name);
            std::snprintf(name, sizeof(name), "BC1 form %u condition %u legacy/shared cycles", rt, condition);
            CheckBits(legacy.cycle == shared.cycle, legacy.cycle, shared.cycle, name);

            const bool take = (rt == 0 || rt == 2) ? !condition : condition;
            const u64 expected_r9 = take ? 20 : 10;
            std::snprintf(name, sizeof(name), "BC1 form %u condition %u branch outcome", rt, condition);
            CheckBits(shared.gpr[9] == expected_r9, shared.gpr[9], expected_r9, name);
            const bool slot_executes = rt < 2 || take;
            const u64 expected_slot = slot_executes ? 1 : 0;
            std::snprintf(name, sizeof(name), "BC1 form %u condition %u delay/annul", rt, condition);
            CheckBits(shared.gpr[10] == expected_slot, shared.gpr[10], expected_slot, name);
        }
    }

    // CTC1 immediately before BC1T must affect the condition sampled by the branch.
    for (bool condition : {false, true})
    {
        GPR_reg initial_gpr[32]{};
        initial_gpr[8].UD[0] = condition ? fcr_mask : 0;
        const u32 branch = MipsI(0x11, 8, 1, 4);
        const std::vector<u32> code = EEBc1ControlProgram(branch, MipsI(9, 0, 10, 1), 1);
        const EESnapshot interp = RunEEProgram(false, code, true, nullptr, nullptr, initial_gpr);
        const EESnapshot legacy = RunEEProgramWithIrMode(0, code, initial_gpr, nullptr, nullptr, nullptr, true);
        const u64 before_shared = EmuCoreXOracleGetEeIrBc1Counts();
        const EESnapshot shared = RunEEProgramWithIrMode(1, code, initial_gpr, nullptr, nullptr, nullptr, true);
        Check(EmuCoreXOracleGetEeIrBc1Counts() > before_shared,
            "CTC1 then BC1 shared production compile counter proves route admission");
        char name[96];
        std::snprintf(name, sizeof(name), "CTC1 then BC1T condition %u interpreter/legacy", condition);
        CompareEE(interp, legacy, name);
        std::snprintf(name, sizeof(name), "CTC1 then BC1T condition %u interpreter/shared", condition);
        CompareEE(interp, shared, name);
        std::snprintf(name, sizeof(name), "CTC1 then BC1T condition %u legacy/shared cycles", condition);
        CheckBits(legacy.cycle == shared.cycle, legacy.cycle, shared.cycle, name);
        const u64 expected_r9 = condition ? 20 : 10;
        std::snprintf(name, sizeof(name), "CTC1 then BC1T condition %u samples updated FCR31", condition);
        CheckBits(shared.gpr[9] == expected_r9, shared.gpr[9], expected_r9, name);
    }

    // A memory operation in the slot verifies ordinary execution and likely annulment.
    for (bool condition : {false, true})
    {
        u32 initial_fcr[32]{};
        if (condition)
            initial_fcr[31] = fcr_mask;
        GPR_reg initial_gpr[32]{};
        initial_gpr[8].UD[0] = 0x89abcdefu;
        initial_gpr[22].UD[0] = EE_TEST_SCRATCH;
        for (u32 rt : {0u, 2u})
        {
            const u32 branch = MipsI(0x11, 8, rt, 4);
            const std::vector<u32> code = EEBc1ControlProgram(branch, MipsI(43, 22, 8, 0));
            const EESnapshot interp = RunEEProgram(false, code, true, nullptr, nullptr, initial_gpr, nullptr, initial_fcr);
            const EESnapshot legacy = RunEEProgramWithIrMode(0, code, initial_gpr, nullptr, nullptr, initial_fcr, true);
            const u64 before_shared = EmuCoreXOracleGetEeIrBc1Counts();
            const EESnapshot shared = RunEEProgramWithIrMode(1, code, initial_gpr, nullptr, nullptr, initial_fcr, true);
            Check(EmuCoreXOracleGetEeIrBc1Counts() > before_shared,
                "BC1 memory delay shared production compile counter proves route admission");
            char name[96];
            std::snprintf(name, sizeof(name), "BC1 memory delay rt%u condition%u interpreter/legacy", rt, condition);
            CompareEE(interp, legacy, name);
            std::snprintf(name, sizeof(name), "BC1 memory delay rt%u condition%u interpreter/shared", rt, condition);
            CompareEE(interp, shared, name);
            std::snprintf(name, sizeof(name), "BC1 memory delay rt%u condition%u legacy/shared cycles", rt, condition);
            CheckBits(legacy.cycle == shared.cycle, legacy.cycle, shared.cycle, name);
            const bool taken = (rt == 0 || rt == 2) ? !condition : condition;
            const bool slot_executes = rt == 0 || taken;
            const u32 actual = shared.scratch[0] | (u32(shared.scratch[1]) << 8) |
                (u32(shared.scratch[2]) << 16) | (u32(shared.scratch[3]) << 24);
            const u32 expected = slot_executes ? 0x89abcdefu : 0x2e211407u;
            std::snprintf(name, sizeof(name), "BC1 memory delay rt%u condition%u write/annul", rt, condition);
            CheckBits(actual == expected, actual, expected, name);
        }
    }

    // COP1 condition-code aliases are reserved by the legacy decoder, and a
    // control transfer in a delay slot remains outside this branch slice.
    for (u32 rt : {4u, 7u, 31u})
    {
        const u32 word = MipsI(0x11, 8, rt, 1);
        ir::Function fn;
        u32 end_pc = 0;
        std::string error;
        char name[96];
        std::snprintf(name, sizeof(name), "ir BC1 reserved Rt %u remains rejected", rt);
        Check(!EeIr::LiftBlock(&word, {EE_TEST_PC, 1}, fn, &end_pc, &error), name);
    }
    for (u32 rs : {3u, 7u, 9u, 31u})
    {
        const u32 word = MipsI(0x11, rs, 0, 1);
        ir::Function fn;
        u32 end_pc = 0;
        std::string error;
        char name[96];
        std::snprintf(name, sizeof(name), "ir non-BC1 COP1 rs %u remains rejected", rs);
        Check(!EeIr::LiftBlock(&word, {EE_TEST_PC, 1}, fn, &end_pc, &error), name);
    }
    const u32 branch = MipsI(0x11, 8, 1, 1);
    const u32 nested_delay = MipsI(0x11, 8, 0, 1);
    const u32 nested_code[] = {branch, nested_delay, 0};
    ir::Function nested;
    u32 end_pc = 0;
    std::string error;
    Check(!EeIr::LiftBlock(nested_code, {EE_TEST_PC, 3}, nested, &end_pc, &error),
        "ir nested BC1 control transfer in delay slot remains rejected");

    // Trap in the slot remains outside this bounded shared slice. Keep the
    // production fallback comparison and assert BC1 did not get admitted.
    const std::vector<u32> exception_code = {MipsI(0x11, 8, 1, 1), MipsR(0, 0, 0, 0, 0x0c), 0};
    ir::Function exception_fn;
    u32 exception_end_pc = 0;
    std::string exception_error;
    Check(!EeIr::LiftBlock(exception_code.data(), {EE_TEST_PC, 3}, exception_fn,
        &exception_end_pc, &exception_error), "ir BC1 syscall delay remains outside this slice");
    const EESnapshot exception_legacy = RunEEProgramWithIrMode(0, exception_code, nullptr, nullptr, nullptr, nullptr, true);
    const u64 before_exception_shared = EmuCoreXOracleGetEeIrBc1Counts();
    const EESnapshot exception_shared = RunEEProgramWithIrMode(1, exception_code, nullptr, nullptr, nullptr, nullptr, true);
    Check(EmuCoreXOracleGetEeIrBc1Counts() == before_exception_shared,
        "BC1 syscall delay fallback does not count shared admission");
    CompareEE(exception_legacy, exception_shared, "BC1 syscall delay legacy/shared exception");
    CheckBits((exception_shared.cp0[13] & 0x80000000u) != 0, exception_shared.cp0[13] & 0x80000000u,
        0x80000000u, "BC1 syscall delay fallback preserves Cause.BD");
}
} // namespace

// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+
#pragma once
#include "ir/Ir.h"
namespace IopIr
{
    // Lift a maximal supported prefix, bounded by max_insts and the current
    // 4 KiB guest page. A supported branch consumes its complete delay slot and
    // ends the sequence; exceptions, coprocessors, nested/unsupported delay slots
    // and IRX import markers remain legacy. The optional flag reports a branch exit.
    // Failure leaves out empty and accepted zero.
    bool LiftSequence(const u32* code, u32 start_pc, u32 max_insts,
        ir::Function& out, u32* accepted, std::string* error, bool* ends_in_branch = nullptr);
}

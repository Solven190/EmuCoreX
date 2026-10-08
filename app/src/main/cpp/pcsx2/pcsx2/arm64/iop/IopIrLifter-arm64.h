// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+
#pragma once
#include "ir/Ir.h"
namespace IopIr
{
    // Lift a maximal supported prefix, bounded by max_insts and the current
    // 4 KiB guest page. Memory, exceptions and control stay on the legacy path.
    // Failure leaves out empty and accepted zero.
    bool LiftSequence(const u32* code, u32 start_pc, u32 max_insts,
        ir::Function& out, u32* accepted, std::string* error);
}

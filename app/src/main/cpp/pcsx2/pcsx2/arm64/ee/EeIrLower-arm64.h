// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0+
#pragma once

// EE source compatibility facade; lowering and allocation are shared with IOP.
#include "arm64/ir/IrLower-arm64.h"
namespace EeIr
{
    using Arm64Ir::LowerHooks;
    using Arm64Ir::LowerOptions;
    using Arm64Ir::LowerOutput;
    using Arm64Ir::CanLower;
    using Arm64Ir::LowerBlock;
}

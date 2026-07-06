// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

namespace VU1Fingerprint
{
    struct KernelEntry
    {
        u64 hash;
        u32 size_bytes;
        const char* name;
        void (*kernel_fn)();
        u32 fake_cycles;
    };

    bool Enabled();
    u64 ComputeHash(const u8* code, size_t bytes);
    void OnUpload(u32 vu_idx, u32 addr, const u8* code, size_t bytes);
    const KernelEntry* OnDispatch(u32 pc);
} // namespace VU1Fingerprint

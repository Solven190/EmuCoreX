// SPDX-License-Identifier: GPL-3.0+

#pragma once

// PCSX2 GS code requires the bundled xxhash with XXH3 support. The Android
// LZ4 dependency also exports an older xxhash.h through its public include
// directory, so keep PCSX2's expected header first without changing upstream.
#include "../../PCSX2/3rdparty/include/xxhash.h"

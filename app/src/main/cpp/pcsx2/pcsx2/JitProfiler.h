#pragma once
#include "common/Pcsx2Defs.h"
#include <vector>
#include <string>

struct JitBlockProfile
{
	u32 startpc;
	u32 size;
	u32 host_size;
	u64 execution_count;
	int type; // 0 = EE, 1 = IOP, 2 = VU0, 3 = VU1
	u64 state_hash = 0; // VU pipeline quick-state key. Zero for EE/IOP.
	u32 variant_index = 0; // VU block variant index for the same start PC.
	u32 flags = 0; // VU metadata/debug flags.
};

namespace JitProfiler
{
	bool IsActive();
	void Start();
	void Stop();
	void EmitBlockIncrement(void* counter_ptr);
}

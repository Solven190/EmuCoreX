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
};

namespace JitProfiler
{
	bool IsActive();
	void Start();
	void Stop();
	void EmitBlockIncrement(void* counter_ptr);
}

#pragma once

#include "common/Pcsx2Defs.h"

namespace HangTrace
{
	enum CpuType : u32
	{
		CPU_EE = 0,
		CPU_IOP = 1,
		CPU_VU0 = 2,
		CPU_VU1 = 3,
	};

	bool IsActive();
	void Start();
	void Stop();
	const char* GetLastReportPath();

	void RecordInterpreter(CpuType cpu, u32 pc, u32 code);
	void RecordJitBlock(u32 cpu, u32 pc, u32 code);
	void EmitBlockTrace(CpuType cpu, u32 pc, u32 code);
}

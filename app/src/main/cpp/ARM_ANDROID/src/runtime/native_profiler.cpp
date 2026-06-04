#include "emucorex/native_profiler.h"

#include <android/trace.h>

#include <atomic>

std::string mVUGetVU1ProfilerStatsAndReset();
std::string mTVUGetProfilerStatsAndReset();

namespace emucorex::android::profiler
{
namespace
{
std::atomic_bool s_enabled{false};
}

void SetEnabled(bool enabled)
{
	s_enabled.store(enabled, std::memory_order_release);
	ATrace_setCounter("EmuCoreX.NativeProfiler", enabled ? 1 : 0);
}

bool IsEnabled()
{
	return s_enabled.load(std::memory_order_relaxed);
}

std::string GetStatus()
{
	std::string status = IsEnabled() ? "native_profiler=on backend=atrace/perfetto" : "native_profiler=off backend=atrace/perfetto";
	const std::string vu1_stats = mVUGetVU1ProfilerStatsAndReset();
	if (!vu1_stats.empty())
	{
		status += ' ';
		status += vu1_stats;
	}
	const std::string mtvu_stats = mTVUGetProfilerStatsAndReset();
	if (!mtvu_stats.empty())
	{
		status += ' ';
		status += mtvu_stats;
	}
	return status;
}

void BeginSection(const char* name)
{
	if (!IsEnabled() || !ATrace_isEnabled())
		return;

	ATrace_beginSection(name);
}

void EndSection()
{
	ATrace_endSection();
}

ScopedSection::ScopedSection(const char* name)
{
	if (!IsEnabled() || !ATrace_isEnabled())
		return;

	active_ = true;
	ATrace_beginSection(name);
}

ScopedSection::~ScopedSection()
{
	if (active_)
		ATrace_endSection();
}
}

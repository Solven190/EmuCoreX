#include "emucorex/native_profiler.h"

#include <android/trace.h>

#include <atomic>

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
	return s_enabled.load(std::memory_order_acquire);
}

std::string GetStatus()
{
	return IsEnabled() ? "native_profiler=on backend=atrace/perfetto" : "native_profiler=off backend=atrace/perfetto";
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

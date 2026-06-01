#pragma once

#include <string>

namespace emucorex::android::profiler
{
void SetEnabled(bool enabled);
bool IsEnabled();
std::string GetStatus();

void BeginSection(const char* name);
void EndSection();

class ScopedSection
{
public:
	explicit ScopedSection(const char* name);
	~ScopedSection();

	ScopedSection(const ScopedSection&) = delete;
	ScopedSection& operator=(const ScopedSection&) = delete;

private:
	bool active_ = false;
};
}

#define EMUCOREX_PROFILE_CONCAT_INNER(a, b) a##b
#define EMUCOREX_PROFILE_CONCAT(a, b) EMUCOREX_PROFILE_CONCAT_INNER(a, b)
#define EMUCOREX_PROFILE_SCOPE(name) \
	::emucorex::android::profiler::ScopedSection EMUCOREX_PROFILE_CONCAT(emucorex_profile_scope_, __LINE__)(name)

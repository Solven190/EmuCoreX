#pragma once

#include "emucorex/android_runtime.h"

namespace emucorex::android
{
using VmStartupCallback = void (*)(void* userdata, bool succeeded);

bool IsUpstreamVmBridgeAvailable();
bool RunUpstreamVm(const VmLaunchConfig& config, VmStartupCallback startup_callback = nullptr, void* startup_userdata = nullptr);
void ApplyRuntimeSettingsToUpstream(const VmLaunchConfig& config);
}

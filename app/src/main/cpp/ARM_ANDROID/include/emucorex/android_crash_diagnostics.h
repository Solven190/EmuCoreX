#pragma once

#include <string>

#include "common/Pcsx2Defs.h"

namespace emucorex::android
{
void RecordVmLaunchForCrashDiagnostics(const std::string& path, bool boot_elf, int probe_steps);
void RecordVmExecutePhaseForCrashDiagnostics(const char* phase);
void RecordGameForCrashDiagnostics(const std::string& title, const std::string& serial, u32 disc_crc, u32 current_crc);

// Set the path to the log file where native crashes will be written.
// Call this once from JNI before starting emulation.
void SetNativeCrashLogFilePath(const char* path);

// Install SIGSEGV/SIGABRT/SIGBUS/SIGFPE handlers that write crash info to the log file.
// Safe to call multiple times — installs only once.
void InstallNativeCrashSignalHandler();
} // namespace emucorex::android

extern "C" void EmuCoreXDumpNativeCrashDiagnostics(
	int signal, void* exception_pc, void* exception_address, bool is_write, int handler_result);

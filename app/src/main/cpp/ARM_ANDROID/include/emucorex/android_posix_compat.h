// SPDX-License-Identifier: GPL-3.0+

#pragma once

#if defined(__ANDROID__)

#include <fcntl.h>

#ifdef __cplusplus
extern "C" int emucorex_android_shm_open(const char* name, int oflag, int mode);
extern "C" int emucorex_android_shm_unlink(const char* name);
#else
int emucorex_android_shm_open(const char* name, int oflag, int mode);
int emucorex_android_shm_unlink(const char* name);
#endif

#define shm_open emucorex_android_shm_open
#define shm_unlink emucorex_android_shm_unlink

#endif

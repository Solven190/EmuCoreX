// SPDX-License-Identifier: GPL-3.0+

#include "emucorex/android_posix_compat.h"

#if defined(__ANDROID__)

#include <cerrno>
#include <cstring>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

#ifndef __NR_memfd_create
#if defined(__aarch64__)
#define __NR_memfd_create 279
#elif defined(__arm__)
#define __NR_memfd_create 385
#elif defined(__i386__)
#define __NR_memfd_create 356
#elif defined(__x86_64__)
#define __NR_memfd_create 319
#endif
#endif

static std::string SanitizeSharedMemoryName(const char* name)
{
	if (!name || !*name)
		return "emucorex-shm";

	std::string result;
	result.reserve(std::strlen(name) + 9);
	result.append("emucorex-");

	for (const char* ch = name; *ch; ch++)
	{
		if (*ch == '/')
		{
			if (!result.empty() && result.back() != '-')
				result.push_back('-');
		}
		else
		{
			result.push_back(*ch);
		}
	}

	if (result == "emucorex-")
		result.append("shm");

	return result;
}

extern "C" int emucorex_android_shm_open(const char* name, int oflag, int /*mode*/)
{
#if defined(__NR_memfd_create)
	if ((oflag & O_CREAT) == 0)
	{
		errno = ENOENT;
		return -1;
	}

	const std::string fd_name = SanitizeSharedMemoryName(name);
	const int fd = static_cast<int>(syscall(__NR_memfd_create, fd_name.c_str(), MFD_CLOEXEC));
	if (fd < 0)
		return -1;

	return fd;
#else
	errno = ENOSYS;
	return -1;
#endif
}

extern "C" int emucorex_android_shm_unlink(const char* /*name*/)
{
	return 0;
}

#endif

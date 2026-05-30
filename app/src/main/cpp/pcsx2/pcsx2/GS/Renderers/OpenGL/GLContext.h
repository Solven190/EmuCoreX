// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"
#include "common/WindowInfo.h"

#include <array>
#include <memory>
#include <vector>

class Error;

class GLContext
{
public:
	GLContext(const WindowInfo& wi);
	virtual ~GLContext();

	struct Version
	{
		enum class Profile
		{
			NoProfile,
			Core,
			ES,
		};

		constexpr Version() = default;

		constexpr Version(int major, int minor)
			: profile(Profile::Core)
			, major_version(major)
			, minor_version(minor)
		{
		}

		constexpr Version(Profile profile_, int major, int minor)
			: profile(profile_)
			, major_version(major)
			, minor_version(minor)
		{
		}

		Profile profile = Profile::Core;
		int major_version = 0;
		int minor_version = 0;
	};

	__fi const WindowInfo& GetWindowInfo() const { return m_wi; }
	__fi bool IsGLES() const { return (m_version.profile == Version::Profile::ES); }
	__fi u32 GetSurfaceWidth() const { return m_wi.surface_width; }
	__fi u32 GetSurfaceHeight() const { return m_wi.surface_height; }

	virtual void* GetProcAddress(const char* name) = 0;
	virtual bool ChangeSurface(const WindowInfo& new_wi) = 0;
	virtual void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) = 0;
	virtual bool SwapBuffers() = 0;
	virtual bool IsCurrent() = 0;
	virtual bool MakeCurrent() = 0;
	virtual bool DoneCurrent() = 0;
	virtual bool SupportsNegativeSwapInterval() const = 0;
	virtual bool SetSwapInterval(s32 interval) = 0;
	virtual std::unique_ptr<GLContext> CreateSharedContext(const WindowInfo& wi, Error* error) = 0;

	static std::unique_ptr<GLContext> Create(const WindowInfo& wi, Error* error);

protected:
	WindowInfo m_wi;
	Version m_version = {};
};

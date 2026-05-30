// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/OpenGL/GLContext.h"

#if defined(_WIN32)
#include "GS/Renderers/OpenGL/GLContextWGL.h"
#else // Linux
#ifdef X11_API
#include "GS/Renderers/OpenGL/GLContextEGLX11.h"
#endif
#ifdef WAYLAND_API
#include "GS/Renderers/OpenGL/GLContextEGLWayland.h"
#endif
#if defined(__ANDROID__)
#include "GS/Renderers/OpenGL/GLContextEGLAndroid.h"
#endif
#endif

#include "common/Console.h"
#include "common/Error.h"

#include "glad/gl.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <span>

static bool ShouldPreferESContext()
{
#ifndef _MSC_VER
	const char* value = std::getenv("PREFER_GLES_CONTEXT");
	return (value && std::strcmp(value, "1") == 0);
#else
	char buffer[2] = {};
	size_t buffer_size = sizeof(buffer);
	getenv_s(&buffer_size, buffer, "PREFER_GLES_CONTEXT");
	return (std::strcmp(buffer, "1") == 0);
#endif
}

static void DisableBrokenExtensions(const char* gl_vendor, const char* gl_renderer)
{
	if (std::strstr(gl_vendor, "ARM"))
	{
		Console.Warning("Mali driver detected, disabling GL_{EXT,OES}_copy_image");
		GLAD_GL_EXT_copy_image = 0;
		GLAD_GL_OES_copy_image = 0;
	}
}

GLContext::GLContext(const WindowInfo& wi)
	: m_wi(wi)
{
}

GLContext::~GLContext() = default;

std::unique_ptr<GLContext> GLContext::Create(const WindowInfo& wi, Error* error)
{
	static constexpr Version vlist[] = {
		{Version::Profile::Core, 4, 6},
		{Version::Profile::Core, 4, 5},
		{Version::Profile::Core, 4, 4},
		{Version::Profile::Core, 4, 3},
		{Version::Profile::Core, 4, 2},
		{Version::Profile::Core, 4, 1},
		{Version::Profile::Core, 4, 0},
		{Version::Profile::Core, 3, 3},
		{Version::Profile::Core, 3, 2},
		{Version::Profile::Core, 3, 1},
		{Version::Profile::Core, 3, 0},
		{Version::Profile::ES, 3, 2},
		{Version::Profile::ES, 3, 1},
		{Version::Profile::ES, 3, 0},
		{Version::Profile::ES, 2, 0},
		{Version::Profile::NoProfile, 0, 0},
	};

	std::array<Version, std::size(vlist)> es_first_vlist;
	std::span<const Version> versions_to_try(vlist);
	if (ShouldPreferESContext())
	{
		size_t count = 0;
		for (const Version& version : vlist)
		{
			if (version.profile == Version::Profile::ES)
				es_first_vlist[count++] = version;
		}
		for (const Version& version : vlist)
		{
			if (version.profile != Version::Profile::ES)
				es_first_vlist[count++] = version;
		}
		versions_to_try = es_first_vlist;
	}

	std::unique_ptr<GLContext> context;
#if defined(_WIN32)
	context = GLContextWGL::Create(wi, versions_to_try, error);
#else // Linux
#if defined(__ANDROID__)
	if (wi.type == WindowInfo::Type::Android)
		context = GLContextEGLAndroid::Create(wi, versions_to_try, error);
#endif

#if defined(X11_API)
	if (wi.type == WindowInfo::Type::X11)
		context = GLContextEGLX11::Create(wi, versions_to_try, error);
#endif

#if defined(WAYLAND_API)
	if (wi.type == WindowInfo::Type::Wayland)
		context = GLContextEGLWayland::Create(wi, versions_to_try, error);
#endif
#endif

	if (!context)
		return nullptr;

	// NOTE: Not thread-safe. But this is okay, since we're not going to be creating more than one context at a time.
	static GLContext* context_being_created;
	context_being_created = context.get();

	const auto loader = [](const char* name) {
		return reinterpret_cast<GLADapiproc>(context_being_created->GetProcAddress(name));
	};
	const int glad_version = context->IsGLES() ? gladLoadGLES2(loader) : gladLoadGL(loader);
	if (!glad_version)
	{
		Error::SetStringView(error, "Failed to load GL functions for GLAD");
		return nullptr;
	}

	context_being_created = nullptr;

	DisableBrokenExtensions(reinterpret_cast<const char*>(glGetString(GL_VENDOR)),
		reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

	return context;
}

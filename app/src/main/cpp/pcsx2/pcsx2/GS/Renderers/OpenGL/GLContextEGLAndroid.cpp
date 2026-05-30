// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/OpenGL/GLContextEGLAndroid.h"

#include "common/Console.h"
#include "common/Error.h"

#include <android/native_window.h>

GLContextEGLAndroid::GLContextEGLAndroid(const WindowInfo& wi)
	: GLContextEGL(wi)
{
}

GLContextEGLAndroid::~GLContextEGLAndroid() = default;

std::unique_ptr<GLContext> GLContextEGLAndroid::Create(const WindowInfo& wi, std::span<const Version> versions_to_try, Error* error)
{
	std::unique_ptr<GLContextEGLAndroid> context = std::make_unique<GLContextEGLAndroid>(wi);
	if (!context->Initialize(versions_to_try, error))
		return nullptr;

	return context;
}

std::unique_ptr<GLContext> GLContextEGLAndroid::CreateSharedContext(const WindowInfo& wi, Error* error)
{
	std::unique_ptr<GLContextEGLAndroid> context = std::make_unique<GLContextEGLAndroid>(wi);
	context->m_display = m_display;

	if (!context->CreateContextAndSurface(m_version, m_context, false))
	{
		Error::SetStringView(error, "Failed to create shared Android GL context/surface");
		return nullptr;
	}

	return context;
}

void GLContextEGLAndroid::ResizeSurface(u32 new_surface_width, u32 new_surface_height)
{
	GLContextEGL::ResizeSurface(new_surface_width, new_surface_height);
}

EGLSurface GLContextEGLAndroid::CreatePlatformSurface(EGLConfig config, void* win, Error* error)
{
	ANativeWindow* native_window = static_cast<ANativeWindow*>(win);
	if (native_window)
	{
		EGLint native_visual_id = 0;
		if (!eglGetConfigAttrib(m_display, config, EGL_NATIVE_VISUAL_ID, &native_visual_id))
		{
			Console.Error("Failed to get native visual ID");
			return EGL_NO_SURFACE;
		}

		ANativeWindow_setBuffersGeometry(native_window, 0, 0, static_cast<int32_t>(native_visual_id));
		m_wi.surface_width = ANativeWindow_getWidth(native_window);
		m_wi.surface_height = ANativeWindow_getHeight(native_window);
	}

	return CreateFallbackSurface(config, win, error);
}

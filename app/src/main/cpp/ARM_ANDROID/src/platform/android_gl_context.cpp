// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/OpenGL/GLContext.h"
#include "GS/Renderers/OpenGL/GLContextEGL.h"

#include "common/Error.h"

#include <android/native_window.h>

#include "glad/gl.h"

GLContext::GLContext(const WindowInfo& wi)
	: m_wi(wi)
{
}

GLContext::~GLContext() = default;

namespace
{
class AndroidGLContext final : public GLContextEGL
{
public:
	using GLContextEGL::GLContextEGL;

	static std::unique_ptr<GLContext> Create(const WindowInfo& wi, std::span<const Version> versions_to_try, Error* error)
	{
		std::unique_ptr<AndroidGLContext> context = std::make_unique<AndroidGLContext>(wi);
		if (!context->Initialize(versions_to_try, error))
			return nullptr;

		return context;
	}

	std::unique_ptr<GLContext> CreateSharedContext(const WindowInfo& wi, Error* error) override
	{
		std::unique_ptr<AndroidGLContext> context = std::make_unique<AndroidGLContext>(wi);
		context->m_display = m_display;

		if (!context->CreateContextAndSurface(m_version, m_context, false))
		{
			Error::SetStringView(error, "Failed to create shared Android GL context/surface");
			return nullptr;
		}

		return context;
	}

protected:
	EGLSurface CreatePlatformSurface(EGLConfig config, void* win, Error* error) override
	{
		ANativeWindow* native_window = static_cast<ANativeWindow*>(win);
		if (native_window)
		{
			EGLint native_visual_id = 0;
			if (eglGetConfigAttrib(m_display, config, EGL_NATIVE_VISUAL_ID, &native_visual_id))
			{
				ANativeWindow_setBuffersGeometry(native_window, 0, 0, static_cast<int32_t>(native_visual_id));
				m_wi.surface_width = ANativeWindow_getWidth(native_window);
				m_wi.surface_height = ANativeWindow_getHeight(native_window);
			}
		}

		return CreateFallbackSurface(config, win, error);
	}
};
} // namespace

std::unique_ptr<GLContext> GLContext::Create(const WindowInfo& wi, Error* error)
{
	static constexpr Version versions_to_try[] = {
		{Version::Profile::ES, 3, 2},
		{Version::Profile::ES, 3, 1},
		{Version::Profile::ES, 3, 0},
		{Version::Profile::ES, 2, 0},
	};

	std::unique_ptr<GLContext> context =
		(wi.type == WindowInfo::Type::Android) ? AndroidGLContext::Create(wi, versions_to_try, error) :
			GLContextEGL::Create(wi, versions_to_try, error);
	if (!context)
		return nullptr;

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
	return context;
}

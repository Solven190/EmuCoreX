#include <android/hardware_buffer.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdatomic.h>

typedef int (*AHardwareBuffer_allocate_fn)(const AHardwareBuffer_Desc*, AHardwareBuffer**);
typedef void (*AHardwareBuffer_describe_fn)(const AHardwareBuffer*, AHardwareBuffer_Desc*);
typedef void (*AHardwareBuffer_acquire_fn)(AHardwareBuffer*);
typedef void (*AHardwareBuffer_release_fn)(AHardwareBuffer*);
typedef const struct native_handle* (*AHardwareBuffer_getNativeHandle_fn)(const AHardwareBuffer*);
typedef int (*AHardwareBuffer_isSupported_fn)(const AHardwareBuffer_Desc*);

static void* g_libandroid;
static atomic_int g_libandroid_loaded;

static void* resolve_android_symbol(const char* name)
{
	if (!atomic_load_explicit(&g_libandroid_loaded, memory_order_acquire))
	{
		void* handle = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
		if (handle)
			g_libandroid = handle;
		atomic_store_explicit(&g_libandroid_loaded, 1, memory_order_release);
	}

	return g_libandroid ? dlsym(g_libandroid, name) : 0;
}

int AHardwareBuffer_allocate(const AHardwareBuffer_Desc* desc, AHardwareBuffer** outBuffer)
{
	AHardwareBuffer_allocate_fn fn =
		(AHardwareBuffer_allocate_fn)resolve_android_symbol("AHardwareBuffer_allocate");
	if (fn)
		return fn(desc, outBuffer);

	if (outBuffer)
		*outBuffer = 0;
	return -ENOSYS;
}

void AHardwareBuffer_describe(const AHardwareBuffer* buffer, AHardwareBuffer_Desc* outDesc)
{
	AHardwareBuffer_describe_fn fn =
		(AHardwareBuffer_describe_fn)resolve_android_symbol("AHardwareBuffer_describe");
	if (fn)
		fn(buffer, outDesc);
}

void AHardwareBuffer_acquire(AHardwareBuffer* buffer)
{
	AHardwareBuffer_acquire_fn fn =
		(AHardwareBuffer_acquire_fn)resolve_android_symbol("AHardwareBuffer_acquire");
	if (fn)
		fn(buffer);
}

void AHardwareBuffer_release(AHardwareBuffer* buffer)
{
	AHardwareBuffer_release_fn fn =
		(AHardwareBuffer_release_fn)resolve_android_symbol("AHardwareBuffer_release");
	if (fn)
		fn(buffer);
}

const struct native_handle* AHardwareBuffer_getNativeHandle(const AHardwareBuffer* buffer)
{
	AHardwareBuffer_getNativeHandle_fn fn =
		(AHardwareBuffer_getNativeHandle_fn)resolve_android_symbol("AHardwareBuffer_getNativeHandle");
	return fn ? fn(buffer) : 0;
}

int AHardwareBuffer_isSupported(const AHardwareBuffer_Desc* desc)
{
	AHardwareBuffer_isSupported_fn fn =
		(AHardwareBuffer_isSupported_fn)resolve_android_symbol("AHardwareBuffer_isSupported");
	return fn ? fn(desc) : 0;
}

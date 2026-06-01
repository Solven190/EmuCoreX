cmake_minimum_required(VERSION 3.22.1)

project(EmuCoreXArmAndroid LANGUAGES C CXX)

set(EMUCOREX_CPP_DIR "${CMAKE_CURRENT_LIST_DIR}/.." CACHE PATH "EmuCoreX native source root")
set(PCSX2_UPSTREAM_DIR "${EMUCOREX_CPP_DIR}/PCSX2" CACHE PATH "Clean PCSX2 upstream checkout")
option(EMUCOREX_LINK_UPSTREAM_PCSX2 "Link the upstream PCSX2 core into libemucore.so" ON)
set(EMUCOREX_OAKNUT_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/third_party/oaknut/include" CACHE PATH "Oaknut include directory")

if(NOT EXISTS "${EMUCOREX_OAKNUT_INCLUDE_DIR}/oaknut/oaknut.hpp")
	message(FATAL_ERROR "Oaknut is required for Phase 2 Android JIT at ${EMUCOREX_OAKNUT_INCLUDE_DIR}")
endif()
set(EMUCOREX_HAS_OAKNUT 1)
set(EMUCOREX_HAS_CAPSTONE 0)

if(NOT EXISTS "${PCSX2_UPSTREAM_DIR}/pcsx2/VMManager.h")
	message(FATAL_ERROR "PCSX2 upstream checkout is missing at ${PCSX2_UPSTREAM_DIR}")
endif()

if(EMUCOREX_LINK_UPSTREAM_PCSX2)
	set(EMUCOREX_THIRD_PARTY_DIR "${CMAKE_CURRENT_LIST_DIR}/third_party")
	set(TOP_CMAKE_WAS_SOURCED TRUE)
	set(ARCH_ARM64 TRUE)
	set(LINUX FALSE)
	set(USE_OPENGL ON CACHE BOOL "" FORCE)
	set(USE_VULKAN ON CACHE BOOL "" FORCE)
	set(USE_VTUNE OFF)
	set(USE_BACKTRACE OFF)
	set(LTO_PCSX2_CORE OFF)
	set(DISABLE_ADVANCE_SIMD OFF)
	set(PCSX2_DEFS
		__POSIX__
		ENABLE_OPENGL
		ENABLE_VULKAN
		OVERRIDE_HOST_PAGE_SIZE=0x1000
		OVERRIDE_HOST_CACHE_LINE_SIZE=64
	)
	set(PCSX2_WARNINGS -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-missing-field-initializers)
	set(FFMPEG_INCLUDE_DIRS "${PCSX2_UPSTREAM_DIR}/3rdparty/ffmpeg/include")
	if(NOT TARGET Threads::Threads)
		add_library(Threads::Threads INTERFACE IMPORTED)
	endif()
	file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/common/include")
	file(WRITE "${CMAKE_BINARY_DIR}/common/include/svnrev.h"
		"#define GIT_HASH \"53e3838c1dd59f611fa3d2fd36915903a1304c6e\"\n"
		"#define GIT_TAG \"v2.7.316\"\n"
		"#define GIT_DATE \"2026-05-06\"\n"
		"#define GIT_REV \"v2.7.316\"\n"
		"#define GIT_TAGGED_COMMIT 1\n"
		"#define GIT_TAG_HI 2\n"
		"#define GIT_TAG_MID 7\n"
		"#define GIT_TAG_LO 316\n"
	)

	function(fixup_file_properties target)
	endfunction()

	function(source_groups_from_vcxproj_filters)
	endfunction()

	function(force_include_last target pattern)
	endfunction()

	function(emucorex_add_interface_target name)
		if(NOT TARGET ${name})
			add_library(${name} INTERFACE IMPORTED)
		endif()
	endfunction()

	function(emucorex_remove_target_sources target)
		get_target_property(_sources ${target} SOURCES)
		if(NOT _sources)
			return()
		endif()
		foreach(_remove IN LISTS ARGN)
			list(FILTER _sources EXCLUDE REGEX "${_remove}$")
		endforeach()
		set_property(TARGET ${target} PROPERTY SOURCES "${_sources}")
	endfunction()

	function(emucorex_add_libjpeg_turbo_static)
		set(_jpeg_dir "${EMUCOREX_THIRD_PARTY_DIR}/libjpeg-turbo")
		set(_jpeg_build_dir "${CMAKE_CURRENT_BINARY_DIR}/third_party/libjpeg-turbo")
		file(MAKE_DIRECTORY "${_jpeg_build_dir}")

		set(VERSION "3.1.91")
		set(COPYRIGHT_YEAR "1991-2026")
		set(BUILD "20260506")
		set(LIBJPEG_TURBO_VERSION_NUMBER 3001091)
		set(JPEG_LIB_VERSION 62)
		set(C_ARITH_CODING_SUPPORTED 1)
		set(D_ARITH_CODING_SUPPORTED 1)
		set(RIGHT_SHIFT_IS_UNSIGNED 0)
		set(HIDDEN "__attribute__((visibility(\"hidden\")))")
		set(INLINE "__inline__ __attribute__((always_inline))")
		set(THREAD_LOCAL "__thread")
		set(SIZE_T 8)
		set(HAVE_BUILTIN_CTZL 1)
		set(SIMD_ARCHITECTURE NONE)
		configure_file("${_jpeg_dir}/src/jversion.h.in" "${_jpeg_build_dir}/jversion.h" @ONLY)
		configure_file("${_jpeg_dir}/src/jconfig.h.in" "${_jpeg_build_dir}/jconfig.h" @ONLY)
		configure_file("${_jpeg_dir}/src/jconfigint.h.in" "${_jpeg_build_dir}/jconfigint.h" @ONLY)

		set(_jpeg_sources
			src/jcapimin.c
			src/wrapper/jcapistd-8.c src/wrapper/jcapistd-12.c src/wrapper/jcapistd-16.c
			src/wrapper/jccoefct-8.c src/wrapper/jccoefct-12.c
			src/wrapper/jccolor-8.c src/wrapper/jccolor-12.c src/wrapper/jccolor-16.c
			src/wrapper/jcdctmgr-8.c src/wrapper/jcdctmgr-12.c
			src/wrapper/jcdiffct-8.c src/wrapper/jcdiffct-12.c src/wrapper/jcdiffct-16.c
			src/jchuff.c src/jcicc.c src/jcinit.c src/jclhuff.c
			src/wrapper/jclossls-8.c src/wrapper/jclossls-12.c src/wrapper/jclossls-16.c
			src/wrapper/jcmainct-8.c src/wrapper/jcmainct-12.c src/wrapper/jcmainct-16.c
			src/jcmarker.c src/jcmaster.c src/jcomapi.c src/jcparam.c src/jcphuff.c
			src/wrapper/jcprepct-8.c src/wrapper/jcprepct-12.c src/wrapper/jcprepct-16.c
			src/wrapper/jcsample-8.c src/wrapper/jcsample-12.c src/wrapper/jcsample-16.c
			src/jctrans.c src/jdapimin.c
			src/wrapper/jdapistd-8.c src/wrapper/jdapistd-12.c src/wrapper/jdapistd-16.c
			src/jdatadst.c src/jdatasrc.c
			src/wrapper/jdcoefct-8.c src/wrapper/jdcoefct-12.c
			src/wrapper/jdcolor-8.c src/wrapper/jdcolor-12.c src/wrapper/jdcolor-16.c
			src/wrapper/jddctmgr-8.c src/wrapper/jddctmgr-12.c
			src/wrapper/jddiffct-8.c src/wrapper/jddiffct-12.c src/wrapper/jddiffct-16.c
			src/jdhuff.c src/jdicc.c src/jdinput.c src/jdlhuff.c
			src/wrapper/jdlossls-8.c src/wrapper/jdlossls-12.c src/wrapper/jdlossls-16.c
			src/wrapper/jdmainct-8.c src/wrapper/jdmainct-12.c src/wrapper/jdmainct-16.c
			src/jdmarker.c src/jdmaster.c
			src/wrapper/jdmerge-8.c src/wrapper/jdmerge-12.c
			src/jdphuff.c
			src/wrapper/jdpostct-8.c src/wrapper/jdpostct-12.c src/wrapper/jdpostct-16.c
			src/wrapper/jdsample-8.c src/wrapper/jdsample-12.c src/wrapper/jdsample-16.c
			src/jdtrans.c src/jerror.c src/jfdctflt.c
			src/wrapper/jfdctfst-8.c src/wrapper/jfdctfst-12.c
			src/wrapper/jfdctint-8.c src/wrapper/jfdctint-12.c
			src/wrapper/jidctflt-8.c src/wrapper/jidctflt-12.c
			src/wrapper/jidctfst-8.c src/wrapper/jidctfst-12.c
			src/wrapper/jidctint-8.c src/wrapper/jidctint-12.c
			src/wrapper/jidctred-8.c src/wrapper/jidctred-12.c
			src/jmemmgr.c src/jmemnobs.c src/jpeg_nbits.c
			src/wrapper/jquant1-8.c src/wrapper/jquant1-12.c
			src/wrapper/jquant2-8.c src/wrapper/jquant2-12.c
			src/wrapper/jutils-8.c src/wrapper/jutils-12.c src/wrapper/jutils-16.c
			src/jaricom.c src/jcarith.c src/jdarith.c
		)
		list(TRANSFORM _jpeg_sources PREPEND "${_jpeg_dir}/")
		add_library(jpeg-static STATIC ${_jpeg_sources})
		set_target_properties(jpeg-static PROPERTIES OUTPUT_NAME jpeg)
		target_include_directories(jpeg-static PUBLIC "${_jpeg_build_dir}" "${_jpeg_dir}/src")
		target_compile_definitions(jpeg-static PUBLIC C_ARITH_CODING_SUPPORTED=1 D_ARITH_CODING_SUPPORTED=1)
		if(NOT TARGET JPEG::JPEG)
			add_library(JPEG::JPEG ALIAS jpeg-static)
		endif()
	endfunction()

	if(NOT TARGET ZLIB::ZLIB)
		add_library(ZLIB::ZLIB INTERFACE IMPORTED)
		target_link_libraries(ZLIB::ZLIB INTERFACE z)
	endif()

	set(SDL_SHARED OFF CACHE BOOL "" FORCE)
	set(SDL_STATIC ON CACHE BOOL "" FORCE)
	set(SDL_TESTS OFF CACHE BOOL "" FORCE)
	set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
	set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
	set(HAVE_WL_VERSION_SCRIPT ON CACHE INTERNAL "Android NDK lld supports linker version scripts")
	add_subdirectory("${EMUCOREX_THIRD_PARTY_DIR}/SDL" "${CMAKE_CURRENT_BINARY_DIR}/third_party/SDL" EXCLUDE_FROM_ALL)

	set(PNG_SHARED OFF CACHE BOOL "" FORCE)
	set(PNG_STATIC ON CACHE BOOL "" FORCE)
	set(PNG_TESTS OFF CACHE BOOL "" FORCE)
	set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
	set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
	add_subdirectory("${EMUCOREX_THIRD_PARTY_DIR}/libpng" "${CMAKE_CURRENT_BINARY_DIR}/third_party/libpng" EXCLUDE_FROM_ALL)
	if(TARGET png_static AND NOT TARGET PNG::PNG)
		add_library(PNG::PNG ALIAS png_static)
	endif()

	set(ENABLE_SHARED OFF CACHE BOOL "" FORCE)
	set(WITH_SIMD OFF CACHE BOOL "" FORCE)
	set(WITH_TURBOJPEG OFF CACHE BOOL "" FORCE)
	emucorex_add_libjpeg_turbo_static()

	set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
	set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
	set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
	set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
	set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
	set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
	set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
	set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
	set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
	add_subdirectory("${EMUCOREX_THIRD_PARTY_DIR}/libwebp" "${CMAKE_CURRENT_BINARY_DIR}/third_party/libwebp" EXCLUDE_FROM_ALL)
	if(TARGET webp AND NOT TARGET WebP::libwebp)
		add_library(WebP::libwebp ALIAS webp)
	endif()

	set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
	set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
	set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
	set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
	add_subdirectory("${EMUCOREX_THIRD_PARTY_DIR}/zstd/build/cmake" "${CMAKE_CURRENT_BINARY_DIR}/third_party/zstd" EXCLUDE_FROM_ALL)
	if(TARGET libzstd_static AND NOT TARGET Zstd::Zstd)
		add_library(Zstd::Zstd ALIAS libzstd_static)
	endif()

	set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
	set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
	set(LZ4_BUNDLED_MODE ON CACHE BOOL "" FORCE)
	add_subdirectory("${EMUCOREX_THIRD_PARTY_DIR}/lz4/build/cmake" "${CMAKE_CURRENT_BINARY_DIR}/third_party/lz4" EXCLUDE_FROM_ALL)
	if(TARGET lz4_static AND NOT TARGET LZ4::LZ4)
		add_library(LZ4::LZ4 ALIAS lz4_static)
	endif()

	set(RYML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(RYML_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
	set(RYML_BUILD_API OFF CACHE BOOL "" FORCE)
	set(RYML_INSTALL OFF CACHE BOOL "" FORCE)
	add_subdirectory("${EMUCOREX_THIRD_PARTY_DIR}/rapidyaml" "${CMAKE_CURRENT_BINARY_DIR}/third_party/rapidyaml" EXCLUDE_FROM_ALL)
	if(TARGET ryml AND NOT TARGET ryml::ryml)
		add_library(ryml::ryml ALIAS ryml)
	endif()

	set(WOLFSSL_INSTALL "no" CACHE STRING "" FORCE)
	set(WOLFSSL_ASM "no" CACHE STRING "" FORCE)
	set(WOLFSSL_CURL "yes" CACHE STRING "" FORCE)
	set(WOLFSSL_TLS13 "yes" CACHE STRING "" FORCE)
	set(WOLFSSL_SINGLE_THREADED "yes" CACHE STRING "" FORCE)
	set(WOLFSSL_EXAMPLES "no" CACHE STRING "" FORCE)
	set(WOLFSSL_CRYPT_TESTS "no" CACHE STRING "" FORCE)
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	add_subdirectory("${EMUCOREX_THIRD_PARTY_DIR}/wolfssl" "${CMAKE_CURRENT_BINARY_DIR}/third_party/wolfssl" EXCLUDE_FROM_ALL)

	set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
	set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
	set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
	set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
	set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
	set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
	set(HTTP_ONLY ON CACHE BOOL "" FORCE)
	set(CURL_USE_PKGCONFIG OFF CACHE BOOL "" FORCE)
	set(CURL_USE_CMAKECONFIG OFF CACHE BOOL "" FORCE)
	set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
	set(CURL_ENABLE_SSL ON CACHE BOOL "" FORCE)
	set(CURL_USE_MBEDTLS OFF CACHE BOOL "" FORCE)
	set(CURL_USE_WOLFSSL ON CACHE BOOL "" FORCE)
	set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
	set(wolfssl_CONFIG TRUE)
	set(WOLFSSL_VERSION "5.9.1")
	set(HAVE_WOLFSSL_GET_PEER_CERTIFICATE TRUE CACHE BOOL "" FORCE)
	set(HAVE_WOLFSSL_USEALPN TRUE CACHE BOOL "" FORCE)
	set(HAVE_WOLFSSL_BIO_NEW FALSE CACHE BOOL "" FORCE)
	set(HAVE_WOLFSSL_BIO_SET_SHUTDOWN FALSE CACHE BOOL "" FORCE)
	set(HAVE_WC_DES_ECBENCRYPT TRUE CACHE BOOL "" FORCE)
	add_subdirectory("${EMUCOREX_THIRD_PARTY_DIR}/curl" "${CMAKE_CURRENT_BINARY_DIR}/third_party/curl" EXCLUDE_FROM_ALL)
	if(TARGET libcurl_static AND NOT TARGET CURL::libcurl)
		add_library(CURL::libcurl ALIAS libcurl_static)
	endif()

	set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
	set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
	set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
	set(FT_DISABLE_HVF ON CACHE BOOL "" FORCE)
	set(FT_REQUIRE_ZLIB OFF CACHE BOOL "" FORCE)
	set(FT_REQUIRE_PNG OFF CACHE BOOL "" FORCE)
	add_subdirectory("${EMUCOREX_THIRD_PARTY_DIR}/freetype" "${CMAKE_CURRENT_BINARY_DIR}/third_party/freetype" EXCLUDE_FROM_ALL)
	if(TARGET freetype AND NOT TARGET Freetype::Freetype)
		add_library(Freetype::Freetype ALIAS freetype)
	endif()

	set(PLUTOVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	add_subdirectory("${EMUCOREX_THIRD_PARTY_DIR}/plutovg" "${CMAKE_CURRENT_BINARY_DIR}/third_party/plutovg" EXCLUDE_FROM_ALL)

	set(PLUTOSVG_ENABLE_FREETYPE OFF CACHE BOOL "" FORCE)
	set(PLUTOSVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	add_subdirectory("${EMUCOREX_THIRD_PARTY_DIR}/plutosvg" "${CMAKE_CURRENT_BINARY_DIR}/third_party/plutosvg" EXCLUDE_FROM_ALL)

	emucorex_add_interface_target(JPEG::JPEG)
	emucorex_add_interface_target(PNG::PNG)
	emucorex_add_interface_target(WebP::libwebp)
	emucorex_add_interface_target(PCAP::PCAP)
	target_include_directories(PCAP::PCAP INTERFACE "${PCSX2_UPSTREAM_DIR}/3rdparty/include")
	emucorex_add_interface_target(X11::X11)
	emucorex_add_interface_target(X11::Xrandr)
	emucorex_add_interface_target(X11::Xi)
	emucorex_add_interface_target(SDL3::SDL3)
	emucorex_add_interface_target(LZ4::LZ4)
	emucorex_add_interface_target(Zstd::Zstd)
	emucorex_add_interface_target(ryml::ryml)

	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/fast_float" "${CMAKE_CURRENT_BINARY_DIR}/upstream/fast_float" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/fmt" "${CMAKE_CURRENT_BINARY_DIR}/upstream/fmt" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/cpuinfo" "${CMAKE_CURRENT_BINARY_DIR}/upstream/cpuinfo" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/lzma" "${CMAKE_CURRENT_BINARY_DIR}/upstream/lzma" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/libchdr" "${CMAKE_CURRENT_BINARY_DIR}/upstream/libchdr" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/libzip" "${CMAKE_CURRENT_BINARY_DIR}/upstream/libzip" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/simpleini" "${CMAKE_CURRENT_BINARY_DIR}/upstream/simpleini" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/soundtouch" "${CMAKE_CURRENT_BINARY_DIR}/upstream/soundtouch" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/imgui" "${CMAKE_CURRENT_BINARY_DIR}/upstream/imgui" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/glad" "${CMAKE_CURRENT_BINARY_DIR}/upstream/glad" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/rcheevos" "${CMAKE_CURRENT_BINARY_DIR}/upstream/rcheevos" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/rapidjson" "${CMAKE_CURRENT_BINARY_DIR}/upstream/rapidjson" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/discord-rpc" "${CMAKE_CURRENT_BINARY_DIR}/upstream/discord-rpc" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/freesurround" "${CMAKE_CURRENT_BINARY_DIR}/upstream/freesurround" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/vulkan" "${CMAKE_CURRENT_BINARY_DIR}/upstream/vulkan" EXCLUDE_FROM_ALL)
	set_property(TARGET freesurround PROPERTY CXX_STANDARD 20)
	set_property(TARGET freesurround PROPERTY CXX_STANDARD_REQUIRED ON)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/demangler" "${CMAKE_CURRENT_BINARY_DIR}/upstream/demangler" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/ccc" "${CMAKE_CURRENT_BINARY_DIR}/upstream/ccc" EXCLUDE_FROM_ALL)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/3rdparty/cubeb" "${CMAKE_CURRENT_BINARY_DIR}/upstream/cubeb" EXCLUDE_FROM_ALL)

	add_subdirectory("${PCSX2_UPSTREAM_DIR}/common" "${CMAKE_CURRENT_BINARY_DIR}/upstream/common")
	emucorex_remove_target_sources(common "DynamicLibrary.cpp" "Linux/LnxHostSys.cpp" "Linux/LnxMisc.cpp")
	target_sources(common PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/src/platform/android_dynamic_library.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/src/platform/android_hostsys.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/src/platform/android_misc.cpp"
	)
	set_property(TARGET common PROPERTY CXX_STANDARD 20)
	set_property(TARGET common PROPERTY CXX_STANDARD_REQUIRED ON)
	add_subdirectory("${PCSX2_UPSTREAM_DIR}/pcsx2" "${CMAKE_CURRENT_BINARY_DIR}/upstream/pcsx2")
	target_link_libraries(PCSX2_FLAGS INTERFACE JPEG::JPEG)
	target_include_directories(PCSX2 PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/include"
		"${EMUCOREX_OAKNUT_INCLUDE_DIR}"
		"${PCSX2_UPSTREAM_DIR}/3rdparty/glad/include"
	)
	target_include_directories(PCSX2 BEFORE PRIVATE
		"${PCSX2_UPSTREAM_DIR}/3rdparty/include"
	)
	emucorex_remove_target_sources(PCSX2
		"Host/SDLAudioStream.cpp"
		"Input/SDLInputSource.cpp"
	)
	target_sources(PCSX2 PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/src/platform/android_aaudio_stream.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/src/platform/android_posix_compat.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/src/platform/android_pcap_unavailable.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/src/platform/android_sdl_input_source.cpp"
		"${PCSX2_UPSTREAM_DIR}/pcsx2/GS/Renderers/OpenGL/GLContextEGL.cpp"
		"${PCSX2_UPSTREAM_DIR}/pcsx2/GS/Renderers/OpenGL/GLContextEGLAndroid.cpp"
	)
	target_include_directories(PCSX2 PRIVATE
		"${CMAKE_ANDROID_NDK}/sources/third_party/shaderc/libshaderc/include"
	)
	target_compile_definitions(PCSX2 PRIVATE VK_USE_PLATFORM_ANDROID_KHR)
	target_compile_definitions(PCSX2 PRIVATE EMUCOREX_ANDROID_JIT=1)
	target_compile_options(PCSX2 PRIVATE
		"SHELL:-include ${CMAKE_CURRENT_LIST_DIR}/include/emucorex/android_posix_compat.h"
	)
	set_property(TARGET PCSX2 PROPERTY CXX_STANDARD 20)
	set_property(TARGET PCSX2 PROPERTY CXX_STANDARD_REQUIRED ON)
endif()

set(EMUCOREX_SHADERC_LIB_DIR
	"${CMAKE_CURRENT_LIST_DIR}/../../../../build/shaderc/out/local/${ANDROID_ABI}"
)
set(EMUCOREX_SHADERC_STATIC_LIBS
	"${EMUCOREX_SHADERC_LIB_DIR}/libshaderc.a"
	"${EMUCOREX_SHADERC_LIB_DIR}/libshaderc_util.a"
	"${EMUCOREX_SHADERC_LIB_DIR}/libglslang.a"
	"${EMUCOREX_SHADERC_LIB_DIR}/libOGLCompiler.a"
	"${EMUCOREX_SHADERC_LIB_DIR}/libOSDependent.a"
	"${EMUCOREX_SHADERC_LIB_DIR}/libSPIRV.a"
	"${EMUCOREX_SHADERC_LIB_DIR}/libHLSL.a"
	"${EMUCOREX_SHADERC_LIB_DIR}/libSPIRV-Tools-opt.a"
	"${EMUCOREX_SHADERC_LIB_DIR}/libSPIRV-Tools.a"
)

add_library(shaderc_shared SHARED
	src/platform/shaderc_shared_anchor.cpp
)
set_target_properties(shaderc_shared PROPERTIES
	OUTPUT_NAME "shaderc_shared"
	LINK_FLAGS "-Wl,-soname,libshaderc_shared.so"
)
target_link_libraries(shaderc_shared PRIVATE
	"-Wl,--whole-archive"
	${EMUCOREX_SHADERC_STATIC_LIBS}
	"-Wl,--no-whole-archive"
)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/third_party/capstone/CMakeLists.txt")
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
	set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "" FORCE)
	set(CAPSTONE_ARM64_SUPPORT ON CACHE BOOL "" FORCE)
	set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
	set(CAPSTONE_BUILD_CSTEST OFF CACHE BOOL "" FORCE)
	set(CAPSTONE_INSTALL OFF CACHE BOOL "" FORCE)
	add_subdirectory(third_party/capstone EXCLUDE_FROM_ALL)
	set(EMUCOREX_HAS_CAPSTONE 1)
endif()

add_library(emucore SHARED
	src/jni/native_app_jni.cpp
	src/jni/support_jni.cpp
	src/platform/android_disc_io.cpp
	src/runtime/android_crash_diagnostics.cpp
	src/runtime/android_input_services.cpp
	src/runtime/android_host_services.cpp
	src/runtime/android_runtime.cpp
	src/runtime/native_profiler.cpp
	src/runtime/upstream_vm_bridge.cpp
)

target_compile_features(emucore PRIVATE cxx_std_20)
target_compile_options(emucore PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(emucore
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/include"
		"${PCSX2_UPSTREAM_DIR}"
		"${PCSX2_UPSTREAM_DIR}/pcsx2"
		"${PCSX2_UPSTREAM_DIR}/common/include"
		"${PCSX2_UPSTREAM_DIR}/3rdparty/fmt/include"
		"${CMAKE_CURRENT_LIST_DIR}/third_party/SDL/include"
)
if(EMUCOREX_HAS_OAKNUT)
	target_include_directories(emucore PRIVATE "${EMUCOREX_OAKNUT_INCLUDE_DIR}")
endif()
if(EMUCOREX_HAS_CAPSTONE)
	target_include_directories(emucore PRIVATE "${CMAKE_CURRENT_LIST_DIR}/third_party/capstone/include")
endif()
target_compile_definitions(emucore
	PRIVATE
		EMUCOREX_PCSX2_TAG="v2.7.316"
		EMUCOREX_PCSX2_COMMIT="53e3838c1dd59f611fa3d2fd36915903a1304c6e"
		EMUCOREX_HAS_OAKNUT=${EMUCOREX_HAS_OAKNUT}
		EMUCOREX_HAS_CAPSTONE=${EMUCOREX_HAS_CAPSTONE}
)
target_link_libraries(emucore PRIVATE aaudio android EGL GLESv3 log)
if(EMUCOREX_HAS_CAPSTONE)
	target_link_libraries(emucore PRIVATE capstone_static)
endif()

if(EMUCOREX_LINK_UPSTREAM_PCSX2)
	target_link_libraries(emucore PRIVATE PCSX2 PCSX2_FLAGS)
endif()

add_dependencies(emucore shaderc_shared)

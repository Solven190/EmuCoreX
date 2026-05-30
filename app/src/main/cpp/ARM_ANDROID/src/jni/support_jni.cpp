#include "emucorex/android_runtime.h"

#include <android/log.h>
#include <jni.h>

using emucorex::android::StringToJString;

namespace
{
constexpr const char* LOG_TAG = "EmuCoreX";

void LogUnsupported(const char* feature)
{
	__android_log_print(ANDROID_LOG_WARN, LOG_TAG, "Unsupported native feature requested in Phase 1: %s", feature);
}
}

extern "C" JNIEXPORT jint JNICALL Java_com_sbro_emucorex_core_utils_SDLControllerManager_nativeSetupJNI(JNIEnv*, jclass) { return 0; }
extern "C" JNIEXPORT jboolean JNICALL Java_com_sbro_emucorex_core_utils_SDLControllerManager_onNativePadDown(JNIEnv*, jclass, jint, jint) { return JNI_FALSE; }
extern "C" JNIEXPORT jboolean JNICALL Java_com_sbro_emucorex_core_utils_SDLControllerManager_onNativePadUp(JNIEnv*, jclass, jint, jint) { return JNI_FALSE; }
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_SDLControllerManager_onNativeJoy(JNIEnv*, jclass, jint, jint, jfloat) {}
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_SDLControllerManager_onNativeHat(JNIEnv*, jclass, jint, jint, jint, jint) {}
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_SDLControllerManager_nativeAddJoystick(JNIEnv*, jclass, jint, jstring, jstring, jint, jint, jint, jint, jint, jint, jboolean) {}
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_SDLControllerManager_nativeRemoveJoystick(JNIEnv*, jclass, jint) {}
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_SDLControllerManager_nativeAddHaptic(JNIEnv*, jclass, jint, jstring) {}
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_SDLControllerManager_nativeRemoveHaptic(JNIEnv*, jclass, jint) {}

extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_hid_HIDDeviceManager_HIDDeviceRegisterCallback(JNIEnv*, jclass) {}
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_hid_HIDDeviceManager_HIDDeviceReleaseCallback(JNIEnv*, jclass) {}
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_hid_HIDDeviceManager_HIDDeviceConnected(JNIEnv*, jclass, jint, jstring, jint, jint, jstring, jint, jstring, jstring, jint, jint, jint, jint, jboolean) {}
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_hid_HIDDeviceManager_HIDDeviceOpenPending(JNIEnv*, jclass, jint) {}
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_hid_HIDDeviceManager_HIDDeviceOpenResult(JNIEnv*, jclass, jint, jboolean) {}
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_hid_HIDDeviceManager_HIDDeviceDisconnected(JNIEnv*, jclass, jint) {}
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_hid_HIDDeviceManager_HIDDeviceInputReport(JNIEnv*, jclass, jint, jbyteArray) {}
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_hid_HIDDeviceManager_HIDDeviceReportResponse(JNIEnv*, jclass, jint, jbyteArray) {}

extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_RetroAchievementsBridge_nativeRequestState(JNIEnv*, jclass) { LogUnsupported("RetroAchievements state bridge"); }
extern "C" JNIEXPORT jstring JNICALL Java_com_sbro_emucorex_core_utils_RetroAchievementsBridge_nativeLogin(JNIEnv* env, jclass, jstring, jstring)
{
	LogUnsupported("RetroAchievements login bridge");
	return StringToJString(env, "RetroAchievements native integration is not linked yet");
}
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_RetroAchievementsBridge_nativeLogout(JNIEnv*, jclass) { LogUnsupported("RetroAchievements logout bridge"); }
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_RetroAchievementsBridge_nativeSetEnabled(JNIEnv*, jclass, jboolean) { LogUnsupported("RetroAchievements enable bridge"); }
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_RetroAchievementsBridge_nativeSetHardcore(JNIEnv*, jclass, jboolean) { LogUnsupported("RetroAchievements hardcore bridge"); }

extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeConfigure(JNIEnv*, jclass, jlong, jstring, jstring, jstring) { LogUnsupported("Discord native bridge"); }
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeProvideStoredToken(JNIEnv*, jclass, jstring, jstring, jstring, jlong, jstring) { LogUnsupported("Discord token bridge"); }
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeBeginAuthorize(JNIEnv*, jclass) { LogUnsupported("Discord authorize bridge"); }
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeSetAppForeground(JNIEnv*, jclass, jboolean) { LogUnsupported("Discord foreground bridge"); }
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativePollCallbacks(JNIEnv*, jclass) { LogUnsupported("Discord callback bridge"); }
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeClearTokens(JNIEnv*, jclass) { LogUnsupported("Discord token-clear bridge"); }
extern "C" JNIEXPORT jboolean JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeIsLoggedIn(JNIEnv*, jclass) { return JNI_FALSE; }
extern "C" JNIEXPORT jboolean JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeIsClientReady(JNIEnv*, jclass) { return JNI_FALSE; }
extern "C" JNIEXPORT jstring JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeConsumeLastError(JNIEnv*, jclass) { return nullptr; }

#include "emucorex/android_runtime.h"

#include <android/log.h>
#include <jni.h>
#include "pcsx2/Achievements.h"
#include "pcsx2/Config.h"
#include "pcsx2/Host.h"
#include "common/Error.h"
#include <mutex>

using emucorex::android::JStringToString;
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

namespace
{
JavaVM* s_java_vm = nullptr;
jclass s_achievements_bridge_class = nullptr;
std::mutex s_achievements_mutex;

void InitializeJniIfNeeded(JNIEnv* env, jclass clazz)
{
	std::lock_guard lock(s_achievements_mutex);
	if (!s_java_vm)
	{
		env->GetJavaVM(&s_java_vm);
	}
	if (!s_achievements_bridge_class)
	{
		s_achievements_bridge_class = static_cast<jclass>(env->NewGlobalRef(clazz));
	}
}

void NotifyStateChanged(
	bool enabled, bool have_user, const char* username, const char* display_name,
	const char* avatar, u32 points, u32 sc_points, u32 unread_messages,
	bool hardcore_pref, bool hardcore_active, bool have_game,
	const char* game_title, const char* rich_presence, const char* icon_path,
	u32 game_id, u32 num_achievements, u32 earned_achievements, u32 score, u32 sc_score,
	bool hardcore_mode, bool leaderboard_enabled, bool rich_presence_enabled
) {
	std::lock_guard lock(s_achievements_mutex);
	if (!s_java_vm || !s_achievements_bridge_class) return;
	JNIEnv* env = nullptr;
	bool attached = false;
	if (s_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
	{
		if (s_java_vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return;
		attached = true;
	}
	jmethodID method = env->GetStaticMethodID(s_achievements_bridge_class, "notifyStateChanged",
		"(ZZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;IIIZZZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;IIIIIZZZ)V");
	if (method)
	{
		jstring j_username = username ? env->NewStringUTF(username) : nullptr;
		jstring j_display_name = display_name ? env->NewStringUTF(display_name) : nullptr;
		jstring j_avatar = avatar ? env->NewStringUTF(avatar) : nullptr;
		jstring j_game_title = game_title ? env->NewStringUTF(game_title) : nullptr;
		jstring j_rich_presence = rich_presence ? env->NewStringUTF(rich_presence) : nullptr;
		jstring j_icon_path = icon_path ? env->NewStringUTF(icon_path) : nullptr;

		env->CallStaticVoidMethod(s_achievements_bridge_class, method,
			static_cast<jboolean>(enabled), static_cast<jboolean>(have_user), j_username, j_display_name, j_avatar,
			static_cast<jint>(points), static_cast<jint>(sc_points), static_cast<jint>(unread_messages),
			static_cast<jboolean>(hardcore_pref), static_cast<jboolean>(hardcore_active), static_cast<jboolean>(have_game),
			j_game_title, j_rich_presence, j_icon_path,
			static_cast<jint>(game_id), static_cast<jint>(num_achievements), static_cast<jint>(earned_achievements),
			static_cast<jint>(score), static_cast<jint>(sc_score),
			static_cast<jboolean>(hardcore_mode), static_cast<jboolean>(leaderboard_enabled), static_cast<jboolean>(rich_presence_enabled)
		);

		if (j_username) env->DeleteLocalRef(j_username);
		if (j_display_name) env->DeleteLocalRef(j_display_name);
		if (j_avatar) env->DeleteLocalRef(j_avatar);
		if (j_game_title) env->DeleteLocalRef(j_game_title);
		if (j_rich_presence) env->DeleteLocalRef(j_rich_presence);
		if (j_icon_path) env->DeleteLocalRef(j_icon_path);
	}
	if (env->ExceptionCheck()) env->ExceptionClear();
	if (attached) s_java_vm->DetachCurrentThread();
}
} // namespace

void NotifyLoginRequested(int reason)
{
	std::lock_guard lock(s_achievements_mutex);
	if (!s_java_vm || !s_achievements_bridge_class) return;
	JNIEnv* env = nullptr;
	bool attached = false;
	if (s_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
	{
		if (s_java_vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return;
		attached = true;
	}
	jmethodID method = env->GetStaticMethodID(s_achievements_bridge_class, "notifyLoginRequested", "(I)V");
	if (method)
	{
		env->CallStaticVoidMethod(s_achievements_bridge_class, method, static_cast<jint>(reason));
	}
	if (env->ExceptionCheck()) env->ExceptionClear();
	if (attached) s_java_vm->DetachCurrentThread();
}

void NotifyLoginSuccess(const char* username, u32 points, u32 sc_points, u32 unread_messages)
{
	std::lock_guard lock(s_achievements_mutex);
	if (!s_java_vm || !s_achievements_bridge_class) return;
	JNIEnv* env = nullptr;
	bool attached = false;
	if (s_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
	{
		if (s_java_vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return;
		attached = true;
	}
	jmethodID method = env->GetStaticMethodID(s_achievements_bridge_class, "notifyLoginSuccess", "(Ljava/lang/String;III)V");
	if (method)
	{
		jstring j_username = username ? env->NewStringUTF(username) : nullptr;
		env->CallStaticVoidMethod(s_achievements_bridge_class, method, j_username, static_cast<jint>(points), static_cast<jint>(sc_points), static_cast<jint>(unread_messages));
		if (j_username) env->DeleteLocalRef(j_username);
	}
	if (env->ExceptionCheck()) env->ExceptionClear();
	if (attached) s_java_vm->DetachCurrentThread();
}

void NotifyHardcoreModeChanged(bool enabled)
{
	std::lock_guard lock(s_achievements_mutex);
	if (!s_java_vm || !s_achievements_bridge_class) return;
	JNIEnv* env = nullptr;
	bool attached = false;
	if (s_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
	{
		if (s_java_vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return;
		attached = true;
	}
	jmethodID method = env->GetStaticMethodID(s_achievements_bridge_class, "notifyHardcoreModeChanged", "(Z)V");
	if (method)
	{
		env->CallStaticVoidMethod(s_achievements_bridge_class, method, static_cast<jboolean>(enabled));
	}
	if (env->ExceptionCheck()) env->ExceptionClear();
	if (attached) s_java_vm->DetachCurrentThread();
}

void NotifySettingsChanged(const char* section, const char* key, const char* value)
{
	std::lock_guard lock(s_achievements_mutex);
	if (!s_java_vm || !s_achievements_bridge_class) return;
	JNIEnv* env = nullptr;
	bool attached = false;
	if (s_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
	{
		if (s_java_vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return;
		attached = true;
	}
	jmethodID method = env->GetStaticMethodID(s_achievements_bridge_class, "notifySettingsChanged", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
	if (method)
	{
		jstring j_section = section ? env->NewStringUTF(section) : nullptr;
		jstring j_key = key ? env->NewStringUTF(key) : nullptr;
		jstring j_value = value ? env->NewStringUTF(value) : nullptr;
		env->CallStaticVoidMethod(s_achievements_bridge_class, method, j_section, j_key, j_value);
		if (j_section) env->DeleteLocalRef(j_section);
		if (j_key) env->DeleteLocalRef(j_key);
		if (j_value) env->DeleteLocalRef(j_value);
	}
	if (env->ExceptionCheck()) env->ExceptionClear();
	if (attached) s_java_vm->DetachCurrentThread();
}

void QueryAndNotifyAchievementsState()
{
	bool enabled = (Host::Internal::GetBaseSettingsLayer() != nullptr) ? Achievements::IsActive() : EmuConfig.Achievements.Enabled;
	const char* username = Achievements::GetLoggedInUserName();
	bool have_user = username != nullptr;
	const char* display_name = Achievements::GetLoggedInUserDisplayName();
	std::string avatar = Achievements::GetLoggedInUserBadgePath();
	u32 points = Achievements::GetLoggedInUserPoints();
	u32 sc_points = Achievements::GetLoggedInUserSoftcorePoints();
	u32 unread_messages = Achievements::GetLoggedInUserUnreadMessages();

	bool hardcore_pref = EmuConfig.Achievements.HardcoreMode;
	bool hardcore_active = Achievements::IsHardcoreModeActive();
	bool have_game = Achievements::HasActiveGame();

	std::string game_title;
	std::string rich_presence;
	std::string icon_path;
	u32 game_id = 0;
	u32 num_achievements = 0;
	u32 earned_achievements = 0;
	u32 score = 0;
	u32 sc_score = 0;

	if (have_game)
	{
		game_title = Achievements::GetGameTitle();
		rich_presence = Achievements::GetRichPresenceString();
		icon_path = Achievements::GetGameIconURL();
		game_id = Achievements::GetGameID();
		num_achievements = Achievements::GetNumAchievements();
		earned_achievements = Achievements::GetEarnedAchievements();
		score = Achievements::GetEarnedPoints();
		sc_score = Achievements::GetGamePoints();
	}

	NotifyStateChanged(
		enabled, have_user, username, display_name,
		avatar.empty() ? nullptr : avatar.c_str(), points, sc_points, unread_messages,
		hardcore_pref, hardcore_active, have_game,
		game_title.empty() ? nullptr : game_title.c_str(),
		rich_presence.empty() ? nullptr : rich_presence.c_str(),
		icon_path.empty() ? nullptr : icon_path.c_str(),
		game_id, num_achievements, earned_achievements, score, sc_score,
		hardcore_pref, EmuConfig.Achievements.LeaderboardNotifications, EmuConfig.Achievements.LeaderboardNotifications
	);
}

extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_RetroAchievementsBridge_nativeRequestState(JNIEnv* env, jclass clazz)
{
	InitializeJniIfNeeded(env, clazz);
	QueryAndNotifyAchievementsState();
}

extern "C" JNIEXPORT jstring JNICALL Java_com_sbro_emucorex_core_utils_RetroAchievementsBridge_nativeLogin(JNIEnv* env, jclass clazz, jstring username, jstring password)
{
	InitializeJniIfNeeded(env, clazz);
	std::string user_str = JStringToString(env, username);
	std::string pass_str = JStringToString(env, password);
	Error error;
	bool success = Achievements::Login(user_str.c_str(), pass_str.c_str(), &error);
	if (success)
	{
		QueryAndNotifyAchievementsState();
		return nullptr;
	}
	else
	{
		std::string err_msg = error.GetDescription();
		if (err_msg.empty()) err_msg = "Login failed";
		return StringToJString(env, err_msg);
	}
}

extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_RetroAchievementsBridge_nativeLogout(JNIEnv* env, jclass clazz)
{
	InitializeJniIfNeeded(env, clazz);
	Achievements::Logout();
	QueryAndNotifyAchievementsState();
}

extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_RetroAchievementsBridge_nativeSetEnabled(JNIEnv* env, jclass clazz, jboolean enabled)
{
	InitializeJniIfNeeded(env, clazz);
	EmuConfig.Achievements.Enabled = (enabled == JNI_TRUE);
	if (Host::Internal::GetBaseSettingsLayer() != nullptr)
	{
		Host::SetBaseBoolSettingValue("Achievements", "Enabled", enabled == JNI_TRUE);
		Host::CommitBaseSettingChanges();
	}
	QueryAndNotifyAchievementsState();
}

extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_RetroAchievementsBridge_nativeSetHardcore(JNIEnv* env, jclass clazz, jboolean enabled)
{
	InitializeJniIfNeeded(env, clazz);
	EmuConfig.Achievements.HardcoreMode = (enabled == JNI_TRUE);
	if (Host::Internal::GetBaseSettingsLayer() != nullptr)
	{
		Host::SetBaseBoolSettingValue("Achievements", "ChallengeMode", enabled == JNI_TRUE);
		Host::CommitBaseSettingChanges();
	}
	if (Achievements::IsActive())
	{
		Achievements::ResetHardcoreMode(false);
	}
	QueryAndNotifyAchievementsState();
}

extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeConfigure(JNIEnv*, jclass, jlong, jstring, jstring, jstring) { LogUnsupported("Discord native bridge"); }
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeProvideStoredToken(JNIEnv*, jclass, jstring, jstring, jstring, jlong, jstring) { LogUnsupported("Discord token bridge"); }
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeBeginAuthorize(JNIEnv*, jclass) { LogUnsupported("Discord authorize bridge"); }
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeSetAppForeground(JNIEnv*, jclass, jboolean) { LogUnsupported("Discord foreground bridge"); }
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativePollCallbacks(JNIEnv*, jclass) { LogUnsupported("Discord callback bridge"); }
extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeClearTokens(JNIEnv*, jclass) { LogUnsupported("Discord token-clear bridge"); }
extern "C" JNIEXPORT jboolean JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeIsLoggedIn(JNIEnv*, jclass) { return JNI_FALSE; }
extern "C" JNIEXPORT jboolean JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeIsClientReady(JNIEnv*, jclass) { return JNI_FALSE; }
extern "C" JNIEXPORT jstring JNICALL Java_com_sbro_emucorex_core_utils_DiscordBridge_nativeConsumeLastError(JNIEnv*, jclass) { return nullptr; }

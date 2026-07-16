#include "common/Error.h"
#include "common/HostSys.h"
#include "emucorex/android_runtime.h"
#include "emucorex/retro_achievements_android.h"
#include "pcsx2/Achievements.h"
#include "pcsx2/Config.h"
#include "pcsx2/Host.h"

#include "fmt/format.h"

#include <cstdlib>
#include <jni.h>
#include <mutex>

using emucorex::android::JStringToString;
using emucorex::android::StringToJString;

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
	bool enabled = EmuConfig.Achievements.Enabled;
	if (Host::Internal::GetBaseSettingsLayer() != nullptr)
		enabled = Host::GetBaseBoolSettingValue("Achievements", "Enabled", enabled);
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
	bool has_leaderboards = false;
	bool has_rich_presence = false;

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
		has_leaderboards = Achievements::HasLeaderboards();
		has_rich_presence = Achievements::HasRichPresence();
	}

	NotifyStateChanged(
		enabled, have_user, username, display_name,
		avatar.empty() ? nullptr : avatar.c_str(), points, sc_points, unread_messages,
		hardcore_pref, hardcore_active, have_game,
		game_title.empty() ? nullptr : game_title.c_str(),
		rich_presence.empty() ? nullptr : rich_presence.c_str(),
		icon_path.empty() ? nullptr : icon_path.c_str(),
		game_id, num_achievements, earned_achievements, score, sc_score,
		hardcore_active, has_leaderboards, has_rich_presence
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
	if (enabled == JNI_TRUE)
	{
		// The Android hub exists before a VM (and sometimes before a base settings
		// layer). RA account/profile requests still need a persistent rcheevos client.
		if (!Achievements::IsActive())
			Achievements::Initialize();
	}
	else if (Achievements::IsActive())
	{
		Achievements::Shutdown(false);
	}
	QueryAndNotifyAchievementsState();
}

extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_RetroAchievementsBridge_nativeSetHardcore(JNIEnv* env, jclass clazz, jboolean enabled)
{
	InitializeJniIfNeeded(env, clazz);
	const bool requested = (enabled == JNI_TRUE);
	EmuConfig.Achievements.HardcoreMode = requested;
	if (Host::Internal::GetBaseSettingsLayer() != nullptr)
	{
		Host::SetBaseBoolSettingValue("Achievements", "ChallengeMode", requested);
		Host::CommitBaseSettingChanges();
	}

	if (Achievements::IsActive() && !requested)
	{
		// Hardcore -> Casual is allowed immediately. Casual -> Hardcore is only
		// activated by VMManager on a full boot/reset, never by this UI call.
		Achievements::DisableHardcoreMode();
	}
	QueryAndNotifyAchievementsState();
}

extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_RetroAchievementsBridge_nativeSetAchievementIndicators(
	JNIEnv* env, jclass clazz, jboolean enabled)
{
	InitializeJniIfNeeded(env, clazz);
	const bool requested = (enabled == JNI_TRUE);
	EmuConfig.Achievements.Overlays = requested;
	if (Host::Internal::GetBaseSettingsLayer() != nullptr)
	{
		Host::SetBaseBoolSettingValue("Achievements", "Overlays", requested);
		Host::CommitBaseSettingChanges();
	}
}

extern "C" JNIEXPORT void JNICALL Java_com_sbro_emucorex_core_utils_RetroAchievementsBridge_nativeSetLeaderboardTrackers(
	JNIEnv* env, jclass clazz, jboolean enabled)
{
	InitializeJniIfNeeded(env, clazz);
	const bool requested = (enabled == JNI_TRUE);
	EmuConfig.Achievements.LBOverlays = requested;
	if (Host::Internal::GetBaseSettingsLayer() != nullptr)
	{
		Host::SetBaseBoolSettingValue("Achievements", "LBOverlays", requested);
		Host::CommitBaseSettingChanges();
	}
}

namespace
{
bool RestartAchievementsClientAfterHostChange()
{
	if (!Achievements::IsActive())
		return true;

	Achievements::Shutdown(false);
	return Achievements::Initialize();
}
} // namespace

extern "C" JNIEXPORT jboolean JNICALL Java_com_sbro_emucorex_core_NativeApp_setAchievementsHostOverride(
	JNIEnv* env, jclass, jstring host)
{
	if (Host::Internal::GetBaseSettingsLayer() == nullptr)
		return JNI_FALSE;

	const std::string host_string = JStringToString(env, host);
	if (host_string.empty())
		return JNI_FALSE;

	// The Android receiver persists the original preference because the base
	// settings layer itself is reconstructed from DataStore after every cold start.
	Host::SetBaseStringSettingValue("Achievements", "Host", host_string.c_str());
	Host::RemoveBaseSettingValue("Achievements", "HostOverrideSavedHardcore");
	Host::SetBaseBoolSettingValue("Achievements", "ChallengeMode", false);
	EmuConfig.Achievements.HardcoreMode = false;
	if (Achievements::IsHardcoreModeActive())
		Achievements::DisableHardcoreMode();
	Host::CommitBaseSettingChanges();

	const bool restarted = RestartAchievementsClientAfterHostChange();
	QueryAndNotifyAchievementsState();
	return restarted ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_sbro_emucorex_core_NativeApp_clearAchievementsHostOverride(
	JNIEnv*, jclass, jint hardcore_restore_mode)
{
	if (Host::Internal::GetBaseSettingsLayer() == nullptr)
		return JNI_FALSE;

	Host::RemoveBaseSettingValue("Achievements", "Host");
	if (hardcore_restore_mode >= 0)
	{
		const bool restore_hardcore = (hardcore_restore_mode != 0);
		Host::SetBaseBoolSettingValue("Achievements", "ChallengeMode", restore_hardcore);
		EmuConfig.Achievements.HardcoreMode = restore_hardcore;
	}
	Host::RemoveBaseSettingValue("Achievements", "HostOverrideSavedHardcore");
	Host::CommitBaseSettingChanges();

	const bool restarted = RestartAchievementsClientAfterHostChange();
	QueryAndNotifyAchievementsState();
	return restarted ? JNI_TRUE : JNI_FALSE;
}

bool emucorex::android::IsRetroAchievementsHardcoreActive()
{
	return Achievements::IsHardcoreModeActive();
}

bool emucorex::android::IsRetroAchievementsHardcoreRequested()
{
	return EmuConfig.Achievements.HardcoreMode || Achievements::IsHardcoreModeActive();
}

bool emucorex::android::IsRetroAchievementsFeatureBlocked(HardcoreRestrictedFeature feature)
{
	switch (feature)
	{
		case HardcoreRestrictedFeature::LoadState:
		case HardcoreRestrictedFeature::Cheats:
		case HardcoreRestrictedFeature::SlowMotion:
		case HardcoreRestrictedFeature::FrameAdvance:
		case HardcoreRestrictedFeature::ResumeState:
		case HardcoreRestrictedFeature::MemoryEditing:
		case HardcoreRestrictedFeature::InputPlayback:
			return IsRetroAchievementsHardcoreActive();
	}

	return true;
}

std::string emucorex::android::GetRetroAchievementsUserAgent()
{
	const char* const app_version = std::getenv("EMUCOREX_APP_VERSION");
	const char* const resolved_app_version =
		(app_version && app_version[0] != '\0') ? app_version : "0.0.0";
	return fmt::format("EmuCoreX/v{} ({}) pcsx2/{}", resolved_app_version, GetOSVersionString(), EMUCOREX_PCSX2_TAG);
}

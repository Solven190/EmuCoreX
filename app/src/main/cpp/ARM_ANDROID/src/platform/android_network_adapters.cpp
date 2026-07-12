#include "emucorex/android_network_adapters.h"

#include <jni.h>

#include <array>
#include <cstdio>

namespace
{
std::mutex s_adapters_mutex;
std::unordered_map<std::string, emucorex::android::network::AdapterInfo> s_adapters;
std::string s_stable_mac = "02:00:00:00:00:01";

std::string ReadString(JNIEnv* env, jobject object, jclass cls, const char* field)
{
	const jfieldID id = env->GetFieldID(cls, field, "Ljava/lang/String;");
	if (!id)
		return {};
	const auto value = static_cast<jstring>(env->GetObjectField(object, id));
	if (!value)
		return {};
	const char* chars = env->GetStringUTFChars(value, nullptr);
	std::string result = chars ? chars : "";
	if (chars)
		env->ReleaseStringUTFChars(value, chars);
	env->DeleteLocalRef(value);
	return result;
}

bool ReadBool(JNIEnv* env, jobject object, jclass cls, const char* field)
{
	const jfieldID id = env->GetFieldID(cls, field, "Z");
	return id && env->GetBooleanField(object, id) == JNI_TRUE;
}

std::vector<std::string> ReadStringArray(JNIEnv* env, jobject object, jclass cls, const char* field)
{
	std::vector<std::string> result;
	const jfieldID id = env->GetFieldID(cls, field, "[Ljava/lang/String;");
	if (!id)
		return result;
	const auto array = static_cast<jobjectArray>(env->GetObjectField(object, id));
	if (!array)
		return result;
	const jsize count = env->GetArrayLength(array);
	result.reserve(count);
	for (jsize i = 0; i < count; i++)
	{
		const auto value = static_cast<jstring>(env->GetObjectArrayElement(array, i));
		if (!value)
			continue;
		const char* chars = env->GetStringUTFChars(value, nullptr);
		if (chars && chars[0] != '\0')
			result.emplace_back(chars);
		if (chars)
			env->ReleaseStringUTFChars(value, chars);
		env->DeleteLocalRef(value);
	}
	env->DeleteLocalRef(array);
	return result;
}
} // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_sbro_emucorex_core_utils_NetworkAdapterCollector_onAdaptersCollected(
	JNIEnv* env, jclass, jobjectArray adapters, jstring stable_mac)
{
	std::lock_guard lock(s_adapters_mutex);
	s_adapters.clear();
	if (stable_mac)
	{
		const char* chars = env->GetStringUTFChars(stable_mac, nullptr);
		if (chars && chars[0] != '\0')
			s_stable_mac = chars;
		if (chars)
			env->ReleaseStringUTFChars(stable_mac, chars);
	}
	if (!adapters)
		return;

	const jsize count = env->GetArrayLength(adapters);
	for (jsize i = 0; i < count; i++)
	{
		jobject adapter_object = env->GetObjectArrayElement(adapters, i);
		if (!adapter_object)
			continue;
		jclass adapter_class = env->GetObjectClass(adapter_object);
		emucorex::android::network::AdapterInfo adapter;
		adapter.name = ReadString(env, adapter_object, adapter_class, "name");
		adapter.dns_servers = ReadStringArray(env, adapter_object, adapter_class, "dnsServers");

		const jfieldID routes_id = env->GetFieldID(adapter_class, "routes",
			"[Lcom/sbro/emucorex/core/utils/NetworkAdapterCollector$AdapterInfo$RouteInfo;");
		const auto routes = routes_id ? static_cast<jobjectArray>(env->GetObjectField(adapter_object, routes_id)) : nullptr;
		if (routes)
		{
			const jsize route_count = env->GetArrayLength(routes);
			adapter.routes.reserve(route_count);
			for (jsize route_index = 0; route_index < route_count; route_index++)
			{
				jobject route_object = env->GetObjectArrayElement(routes, route_index);
				if (!route_object)
					continue;
				jclass route_class = env->GetObjectClass(route_object);
				emucorex::android::network::RouteInfo route;
				route.gateway = ReadString(env, route_object, route_class, "gateway");
				route.is_ipv6 = ReadBool(env, route_object, route_class, "isIPv6");
				route.has_gateway = ReadBool(env, route_object, route_class, "hasGateway");
				route.is_default = ReadBool(env, route_object, route_class, "isDefault");
				adapter.routes.push_back(std::move(route));
				env->DeleteLocalRef(route_class);
				env->DeleteLocalRef(route_object);
			}
			env->DeleteLocalRef(routes);
		}
		if (!adapter.name.empty())
			s_adapters.insert_or_assign(adapter.name, std::move(adapter));
		env->DeleteLocalRef(adapter_class);
		env->DeleteLocalRef(adapter_object);
	}
}

std::vector<std::string> emucorex::android::network::GetGateways(const std::string& adapter_name)
{
	std::lock_guard lock(s_adapters_mutex);
	const auto found = s_adapters.find(adapter_name);
	if (found == s_adapters.end())
		return {};
	std::vector<std::string> result;
	for (const RouteInfo& route : found->second.routes)
	{
		if (!route.is_ipv6 && route.has_gateway && route.is_default && !route.gateway.empty())
			result.push_back(route.gateway);
	}
	return result;
}

std::vector<std::string> emucorex::android::network::GetDnsServers(const std::string& adapter_name)
{
	std::lock_guard lock(s_adapters_mutex);
	const auto found = s_adapters.find(adapter_name);
	return found == s_adapters.end() ? std::vector<std::string>{} : found->second.dns_servers;
}

std::string emucorex::android::network::GetStableMacAddress()
{
	std::lock_guard lock(s_adapters_mutex);
	return s_stable_mac;
}

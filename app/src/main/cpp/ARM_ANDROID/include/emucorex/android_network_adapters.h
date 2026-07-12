#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace emucorex::android::network
{
struct RouteInfo
{
	std::string gateway;
	bool is_ipv6 = false;
	bool has_gateway = false;
	bool is_default = false;
};

struct AdapterInfo
{
	std::string name;
	std::vector<std::string> dns_servers;
	std::vector<RouteInfo> routes;
};

std::vector<std::string> GetGateways(const std::string& adapter_name);
std::vector<std::string> GetDnsServers(const std::string& adapter_name);
std::string GetStableMacAddress();
} // namespace emucorex::android::network

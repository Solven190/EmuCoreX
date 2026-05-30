// SPDX-License-Identifier: GPL-3.0+

#include "pcsx2/Input/InputManager.h"

#include "common/StringUtil.h"

#include <charconv>
#include <optional>
#include <string>

namespace
{
std::optional<u32> ParseAndroidKeyCode(std::string_view str)
{
	if (str.starts_with("Android/"))
		str.remove_prefix(8);
	else if (str.starts_with("KeyCode"))
		str.remove_prefix(7);

	u32 value = 0;
	const char* begin = str.data();
	const char* end = str.data() + str.size();
	const auto result = std::from_chars(begin, end, value);
	if (result.ec != std::errc() || result.ptr != end)
		return std::nullopt;

	return value;
}
}

std::optional<u32> InputManager::ConvertHostKeyboardStringToCode(const std::string_view str)
{
	return ParseAndroidKeyCode(StringUtil::StripWhitespace(str));
}

std::optional<std::string> InputManager::ConvertHostKeyboardCodeToString(u32 code)
{
	return std::string("Android/") + std::to_string(code);
}

const char* InputManager::ConvertHostKeyboardCodeToIcon(u32)
{
	return nullptr;
}

BEGIN_HOTKEY_LIST(g_host_hotkeys)
END_HOTKEY_LIST()

#include <stdint.h>
#include <string.h>
#include <sys/system_properties.h>

int property_get(const char* key, char* value, const char* default_value)
{
	int length = __system_property_get(key, value);
	if (length > 0)
		return length;

	if (default_value)
	{
		strcpy(value, default_value);
		return (int)strlen(default_value);
	}

	if (value)
		value[0] = '\0';

	return 0;
}

void atrace_begin_body(const char* name)
{
	(void)name;
}

void atrace_end_body(void)
{
}

void atrace_init(void)
{
}

uint64_t atrace_get_enabled_tags(void)
{
	return 0;
}

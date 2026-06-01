#include <errno.h>

int hw_get_module(const char* id, const void** module)
{
	(void)id;
	if (module)
		*module = 0;
	return -ENOENT;
}

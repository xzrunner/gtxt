#ifdef __cplusplus
extern "C"
{
#endif

#ifndef gametext_util_h
#define gametext_util_h

int gtxt_unicode_len(const char chr);
int gtxt_get_unicode(const char* str, int n);

#ifdef _MSC_VER
#	include <malloc.h>
#	define ARRAY(type, name, size) type* name = (type*)_alloca((size) * sizeof(type))
#else
#	define ARRAY(type, name, size) type name[size]
#endif

#endif // gametext_util_h

#ifdef __cplusplus
}
#endif
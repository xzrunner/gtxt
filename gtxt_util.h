#ifdef __cplusplus
extern "C"
{
#endif

#ifndef gametext_util_h
#define gametext_util_h

int gtxt_unicode_len(const char chr);
int gtxt_get_unicode(const char* str, int n);

#endif // gametext_util_h

#ifdef __cplusplus
}
#endif
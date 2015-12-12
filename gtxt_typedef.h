#ifdef __cplusplus
extern "C"
{
#endif

#ifndef gametext_typedef_h
#define gametext_typedef_h

#include <stdint.h>

#if (('1234' >> 24) == '1')
#elif (('4321' >> 24) == '1')
  #define BIG_ENDIAN
#else
  #error "Couldn't determine the endianness!"
#endif

union gtxt_color {
	uint32_t integer;
	struct
	{
#ifdef BIG_ENDIAN
		uint8_t r, g, b, a;
#else
		uint8_t a, b, g, r;
#endif
	};
};

#define MAX(a, b) ( ((a)>(b))?(a):(b) )
#define MIN(a, b) ( ((a)<(b))?(a):(b) )

#endif // gametext_typedef_h

#ifdef __cplusplus
}
#endif
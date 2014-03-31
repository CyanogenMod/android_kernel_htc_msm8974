#define BITS64

typedef char flag;
typedef unsigned char uint8;
typedef signed char int8;
typedef int uint16;
typedef int int16;
typedef unsigned int uint32;
typedef signed int int32;
#ifdef BITS64
typedef unsigned long long int bits64;
typedef signed long long int sbits64;
#endif

typedef unsigned char bits8;
typedef signed char sbits8;
typedef unsigned short int bits16;
typedef signed short int sbits16;
typedef unsigned int bits32;
typedef signed int sbits32;
#ifdef BITS64
typedef unsigned long long int uint64;
typedef signed long long int int64;
#endif

#ifdef BITS64
#define LIT64( a ) a##LL
#endif

#define INLINE static inline



#ifdef __LIBFLOAT__

#define float32_add			__addsf3
#define float32_sub			__subsf3
#define float32_mul			__mulsf3
#define float32_div			__divsf3
#define int32_to_float32		__floatsisf
#define float32_to_int32_round_to_zero	__fixsfsi
#define float32_to_uint32_round_to_zero	__fixunssfsi

#define float32_eq			___float32_eq
#define float32_le			___float32_le
#define float32_lt			___float32_lt

#define float64_add			___float64_add
#define float64_sub			___float64_sub
#define float64_mul			___float64_mul
#define float64_div			___float64_div
#define int32_to_float64		___int32_to_float64
#define float64_to_int32_round_to_zero	___float64_to_int32_round_to_zero
#define float64_to_uint32_round_to_zero	___float64_to_uint32_round_to_zero
#define float64_to_float32		___float64_to_float32
#define float32_to_float64		___float32_to_float64
#define float64_eq			___float64_eq
#define float64_le			___float64_le
#define float64_lt			___float64_lt

#if 0
#define float64_add			__adddf3
#define float64_sub			__subdf3
#define float64_mul			__muldf3
#define float64_div			__divdf3
#define int32_to_float64		__floatsidf
#define float64_to_int32_round_to_zero	__fixdfsi
#define float64_to_uint32_round_to_zero	__fixunsdfsi
#define float64_to_float32		__truncdfsf2
#define float32_to_float64		__extendsfdf2
#endif

#endif

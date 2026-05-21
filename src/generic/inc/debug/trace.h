#ifndef DEBUG_TRACE_H_
#define DEBUG_TRACE_H_

#include <log/log.h>

#define TRACE_PREFIX "%s:%u:%u: "
#define TRACE_ARGS , __FILE__, __LINE__, logger_timestamp()

#define NO_TRACE do {} while (0)
#define NO_DUMP  do {} while (0)

#if defined(TRACE_EN)
#if TRACE_EN
#define TRACE_VAR(...) __VA_ARGS__
#ifndef _MSC_VER
#define TRACE(fmt, ...) LOG(TRACE_PREFIX fmt TRACE_ARGS, ## __VA_ARGS__)
#else
#define TRACE(...) do { LOG(TRACE_PREFIX TRACE_ARGS); LOG(__VA_ARGS__); } while (0)
#endif
#else
#define TRACE_VAR(...)
#define TRACE(...)      NO_TRACE
#endif
#else
#define TRACE_VAR(...)
#define TRACE(...)      NO_TRACE
#endif

#if defined(DUMP_EN)
#if DUMP_EN
#define DUMP_VAR(...)   __VA_ARGS__
#define DUMP(...)       ::dump(__VA_ARGS__)
#define DUMPF(...)      ::dumpf(__VA_ARGS__)
#define DUMPD(...)      ::dumpd(__VA_ARGS__)
#define DUMPI(...)      ::dumpi(__VA_ARGS__)
#define DUMPU(...)      ::dumpu(__VA_ARGS__)
#define DUMPH(...)      ::dumph(__VA_ARGS__)
#define DUMPUH(...)     ::dumpuh(__VA_ARGS__)
#define DUMPUL(...)     ::dumpul(__VA_ARGS__)
#define DUMPXI(...) ::dumpxi(__VA_ARGS__)
#define DUMPXU(...) ::dumpxu(__VA_ARGS__)
#define DUMPXL(...) ::dumpxl(__VA_ARGS__)
#define DUMPXH(...) ::dumpxh(__VA_ARGS__)
#define DUMPXUL(...) ::dumpxul(__VA_ARGS__)
#define DUMPXUH(...) ::dumpxuh(__VA_ARGS__)
#else
#define DUMP_VAR(...)
#define DUMP(...)       NO_DUMP
#define DUMPF(...)      NO_DUMP
#define DUMPD(...)      NO_DUMP
#define DUMPI(...)      NO_DUMP
#define DUMPU(...)      NO_DUMP
#define DUMPH(...)      NO_DUMP
#define DUMPH(...)      NO_DUMP
#define DUMPUH(...)     NO_DUMP
#define DUMPUL(...)     NO_DUMP
#define DUMPXI(...)     NO_DUMP
#define DUMPXU(...)     NO_DUMP
#define DUMPXL(...)     NO_DUMP
#define DUMPXH(...)     NO_DUMP
#define DUMPXUL(...)    NO_DUMP
#define DUMPXUH(...)    NO_DUMP
#endif
#else
#define DUMP(...)       NO_DUMP
#define DUMPF(...)      NO_DUMP
#define DUMPD(...)      NO_DUMP
#define DUMPI(...)      NO_DUMP
#define DUMPU(...)      NO_DUMP
#define DUMPH(...)      NO_DUMP
#define DUMPUH(...)     NO_DUMP
#define DUMPUL(...)     NO_DUMP
#define DUMPXI(...)     NO_DUMP
#define DUMPXU(...)     NO_DUMP
#define DUMPXL(...)     NO_DUMP
#define DUMPXH(...)     NO_DUMP
#define DUMPXUL(...)    NO_DUMP
#define DUMPXUH(...)    NO_DUMP
#endif


#ifdef TRACE_ME
#define LTRACE_VAR(...) TRACE_VAR(__VA_ARGS__)
#define LTRACE(...)     TRACE(__VA_ARGS__)
#define LDUMP_VAR(...)  DUMP_VAR(__VA_ARGS__)
#define LDUMP(...)      DUMP(__VA_ARGS__)
#define LDUMPF(...)     DUMPF(__VA_ARGS__)
#define LDUMPD(...)     DUMPD(__VA_ARGS__)
#define LDUMPI(...)     DUMPI(__VA_ARGS__)
#define LDUMPU(...)     DUMPU(__VA_ARGS__)
#define LDUMPH(...)     DUMPH(__VA_ARGS__)
#define LDUMPUH(...)    DUMPUH(__VA_ARGS__)
#define LDUMPUL(...)    DUMPUL(__VA_ARGS__)
#define LDUMPXI(...) DUMPXI(__VA_ARGS__)
#define LDUMPXU(...) DUMPXU(__VA_ARGS__)
#define LDUMPXL(...) DUMPXL(__VA_ARGS__)
#define LDUMPXH(...) DUMPXH(__VA_ARGS__)
#define LDUMPXUL(...) DUMPXUL(__VA_ARGS__)
#define LDUMPXUH(...) DUMPXUH(__VA_ARGS__)
#else
#define LTRACE_VAR(...)
#define LTRACE(...)     NO_TRACE
#define LDUMP_VAR(...)
#define LDUMP(...)      NO_DUMP
#define LDUMPF(...)     NO_DUMP
#define LDUMPD(...)     NO_DUMP
#define LDUMPI(...)     NO_DUMP
#define LDUMPU(...)     NO_DUMP
#define LDUMPH(...)     NO_DUMP
#define LDUMPUH(...)    NO_DUMP
#define LDUMPUL(...)    NO_DUMP
#define LDUMPXI(...)    NO_DUMP
#define LDUMPXU(...)    NO_DUMP
#define LDUMPXL(...)    NO_DUMP
#define LDUMPXH(...)    NO_DUMP
#define LDUMPXUL(...)   NO_DUMP
#define LDUMPXUH(...)   NO_DUMP
#endif

EXTERN_C void dump(const char * banner, size_t addr, const void * data, size_t size, size_t addr_step);
EXTERN_C void dumpf(const char * banner, size_t addr, const float * data, size_t count);
EXTERN_C void dumpd(const char * banner, size_t addr, const double * data, size_t count);
EXTERN_C void dumpl(const char * banner, size_t addr, const long * data, size_t count);
EXTERN_C void dumpul(const char * banner, size_t addr, const ulong * data, size_t count);
EXTERN_C void dumpi(const char * banner, size_t addr, const int * data, size_t count);
EXTERN_C void dumpu(const char * banner, size_t addr, const uint * data, size_t count);
EXTERN_C void dumph(const char * banner, size_t addr, const short * data, size_t count);
EXTERN_C void dumpuh(const char * banner, size_t addr, const ushort * data, size_t count);

EXTERN_C void dumpxl(const char * banner, size_t addr, const long * data, size_t count);
EXTERN_C void dumpxul(const char * banner, size_t addr, const ulong * data, size_t count);
EXTERN_C void dumpxi(const char * banner, size_t addr, const int * data, size_t count);
EXTERN_C void dumpxu(const char * banner, size_t addr, const uint * data, size_t count);
EXTERN_C void dumpxh(const char * banner, size_t addr, const short * data, size_t count);
EXTERN_C void dumpxuh(const char * banner, size_t addr, const ushort * data, size_t count);


#ifdef __cplusplus
inline void dump(const char * banner, const void * data, size_t size) { dump(banner, 0, data, size, 16); }
inline void dump(const char * banner, size_t addr, const void * data, size_t size) { dump(banner, addr, data, size, 16); }
inline void dump(size_t addr, const void * data, size_t size, size_t addr_step) { dump(nullptr, addr, data, size, addr_step); }
inline void dump(size_t addr, const void * data, size_t size) { dump(nullptr, addr, data, size, 16); }
inline void dump(const void * data, size_t size) { dump(nullptr, 0, data, size, 16); }

template <class T> void dump(const char * banner, const T& data) { dump(banner, 0, &data, sizeof(data), 16); }
template <class T> void dump(const T& data) { dump(nullptr, 0, &data, sizeof(data), 16); }

inline void dumpf(const char * banner, const float * data, size_t count) { dumpf(banner, 0, data, count); }
inline void dumpf(size_t addr, const float * data, size_t count) { dumpf(nullptr, addr, data, count); }
inline void dumpf(const float * data, size_t count) { dumpf(nullptr, 0, data, count); }

inline void dumpd(const char * banner, const double * data, size_t count) { dumpd(banner, 0, data, count); }
inline void dumpd(size_t addr, const double * data, size_t count) { dumpd(nullptr, addr, data, count); }
inline void dumpd(const double * data, size_t count) { dumpd(nullptr, 0, data, count); }

inline void dumpl(const char * banner, const long * data, size_t count) { dumpl(banner, 0, data, count); }
inline void dumpl(size_t addr, const long * data, size_t count) { dumpl(nullptr, addr, data, count); }
inline void dumpl(const long * data, size_t count) { dumpl(nullptr, 0, data, count); }

inline void dumpul(const char * banner, const ulong * data, size_t count) { dumpul(banner, 0, data, count); }
inline void dumpul(size_t addr, const ulong * data, size_t count) { dumpul(nullptr, addr, data, count); }
inline void dumpul(const ulong * data, size_t count) { dumpul(nullptr, 0, data, count); }

inline void dumpi(const char * banner, const int * data, size_t count) { dumpi(banner, 0, data, count); }
inline void dumpi(size_t addr, const int * data, size_t count) { dumpi(nullptr, addr, data, count); }
inline void dumpi(const int * data, size_t count) { dumpi(nullptr, 0, data, count); }

inline void dumpu(const char * banner, const uint * data, size_t count) { dumpu(banner, 0, data, count); }
inline void dumpu(size_t addr, const uint * data, size_t count) { dumpu(nullptr, addr, data, count); }
inline void dumpu(const uint * data, size_t count) { dumpu(nullptr, 0, data, count); }

inline void dumph(const char * banner, const short * data, size_t count) { dumph(banner, 0, data, count); }
inline void dumph(size_t addr, const short * data, size_t count) { dumph(nullptr, addr, data, count); }
inline void dumph(const short * data, size_t count) { dumph(nullptr, 0, data, count); }

inline void dumpuh(const char * banner, const ushort * data, size_t count) { dumpuh(banner, 0, data, count); }
inline void dumpuh(size_t addr, const ushort * data, size_t count) { dumpuh(nullptr, addr, data, count); }
inline void dumpuh(const ushort * data, size_t count) { dumpuh(nullptr, 0, data, count); }

inline void dumpxl(const char * banner, const long * data, size_t count) { dumpxl(banner, 0, data, count); }
inline void dumpxl(size_t addr, const long * data, size_t count) {dumpxl(nullptr, addr, data, count); }
inline void dumpxl(const long * data, size_t count) { dumpxl(nullptr, 0, data, count); }

inline void dumpxul(const char * banner, const ulong * data, size_t count) { dumpxul(banner, 0, data, count); }
inline void dumpxul(size_t addr, const ulong * data, size_t count) { dumpxul(nullptr, addr, data, count); }
inline void dumpxul(const ulong * data, size_t count) { dumpxul(nullptr, 0, data, count); }

inline void dumpxi(const char * banner, const int * data, size_t count)  { dumpxi(banner, 0, data, count); }
inline void dumpxi(size_t addr, const int * data, size_t count) { dumpxi(nullptr, addr, data, count); }
inline void dumpxi(const int * data, size_t count) { dumpxi(nullptr, 0, data, count); }

inline void dumpxu(const char * banner, const uint * data, size_t count)  { dumpxu(banner, 0, data, count); }
inline void dumpxu(size_t addr, const uint * data, size_t count) { dumpxu(nullptr, addr, data, count); }
inline void dumpxu(const uint * data, size_t count) { dumpxu(nullptr, 0, data, count); }

inline void dumpxh(const char * banner, const short * data, size_t count)  { dumpxh(banner, 0, data, count); }
inline void dumpxh(size_t addr, const short * data, size_t count) { dumpxh(nullptr, addr, data, count); }
inline void dumpxh(const short * data, size_t count) { dumpxh(nullptr, 0, data, count); }

inline void dumpxuh(const char * banner, const ushort * data, size_t count)  { dumpxuh(banner, 0, data, count); }
inline void dumpxuh(size_t addr, const ushort * data, size_t count) { dumpxuh(nullptr, addr, data, count); }
inline void dumpxuh(const ushort * data, size_t count) { dumpxuh(nullptr, 0, data, count); }

#endif // __cplusplus
#endif /* TRACE_H_ */
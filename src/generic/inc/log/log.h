#ifndef LOG_LOG_H_
#define LOG_LOG_H_

#define LOG_ERROR_PREFIX "%s:%u:%u: "
#define MAKE_LOG_ERROR_ARGS(file, line, time) , file, line, time
#define LOG_ERROR_ARGS MAKE_LOG_ERROR_ARGS(__FILE__, __LINE__, logger_timestamp())

#define NO_LOG do {} while (0)

#ifdef __cplusplus
#ifndef _MSC_VER
#define LOG_ERROR(fmt, ...) ::logfn(LogKind::NV_ERROR, LOG_ERROR_PREFIX fmt LOG_ERROR_ARGS, ## __VA_ARGS__)
#else
#define LOG_ERROR(...) do { ::logfn(LogKind::NV_ERROR, LOG_ERROR_PREFIX LOG_ERROR_ARGS); ::logfn(LogKind::NV_ERROR, __VA_ARGS__); } while (0) // MSVC has bug with __VA_ARGS__
#endif
#define VLOG(level, str, args) ::vlogfn(level, str, args)
#define LOG(...) ::logfn(LogKind::GENERIC, __VA_ARGS__)
#define NV_LOG(...) ::logfn(LogKind::NV_LOG, __VA_ARGS__)
#else
#ifndef _MSC_VER
#define LOG_ERROR(fmt, ...) logfn(LOG_KIND_NV_ERROR, LOG_ERROR_PREFIX fmt LOG_ERROR_ARGS, ## __VA_ARGS__)
#else
#define LOG_ERROR(...) do { logfn(LOG_KIND_ERROR, LOG_ERROR_PREFIX LOG_ERROR_ARGS); logfn(LOG_KIND_ERROR, __VA_ARGS__); } while (0) // MSVC has bug with __VA_ARGS__
#endif
#define VLOG(level, str, args) vlogfn(level, str, args)
#define LOG(...) logfn(LOG_KIND_GENERIC, __VA_ARGS__)
#define NV_LOG(...) ::logfn(LOG_KIND_NV_LOG, __VA_ARGS__)
#endif

#ifdef __cplusplus
enum class LogKind { NV_ERROR, NV_LOG, GENERIC }; // here need to select something not used to avoid collisions ...
#else
typedef enum { LOG_KIND_NV_ERROR, LOG_KING_NV_LOG, LOG_KIND_GENERIC } LogKind;
#endif

typedef void LogFnV(LogKind level, const char * fmt, va_list args);
typedef /*PRINTF_FORMAT(2,3)*/ void LogFn(LogKind level, const char * fmt, ...);
typedef LogFnV Logger;

EXTERN_C uint logger_timestamp();
EXTERN_C void set_logger(Logger * logger);
EXTERN_C Logger * get_logger(void);

//
// names log, logf and logl are already used by math.h
//
EXTERN_C LogFn logfn;
EXTERN_C LogFnV vlogfn;


#endif /* LOG_LOG_H_ */
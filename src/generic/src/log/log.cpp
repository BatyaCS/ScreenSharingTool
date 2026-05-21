#include <common.h>
#include <log/log.h>

static void null_logger(LogKind level, const char * fmt, va_list args);

static Logger * logger = &null_logger;

void set_logger(Logger* logger)
{
    ::logger = logger ? logger : null_logger;
}

Logger* get_logger()
{
    Logger * lgr = ::logger;
    return &null_logger == lgr ? 0 : lgr;
}

EXTERN_C void vlogfn(LogKind level, const char* fmt, va_list args)
{
    ::logger(level, fmt, args);
}

EXTERN_C void logfn(LogKind level, const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    VLOG(level, fmt, args);
    va_end(args);
}

void null_logger(LogKind level, const char * fmt, va_list args)
{}
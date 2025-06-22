#include <CZ/Ream/RLog.h>
#include <algorithm>
#include <cstdlib>
#include <stdio.h>
#include <stdarg.h>

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#define BRELN "\n"

int level = 0;
int eglLevel = 0;

void CZ::RLogInit() noexcept
{
    char *env = getenv("REAM_DEBUG");

    if (env)
        level = std::clamp(atoi(env), 0, 4);
    else
        level = 0;

    env = getenv("REAM_EGL_DEBUG");

    if (env)
        eglLevel = std::clamp(atoi(env), 0, 4);
    else
        eglLevel = 0;
}

void CZ::RFatal(const char *format, ...) noexcept
{
    if (level >= 1)
    {
        fprintf(stderr, "%sReam fatal:%s ", KRED, KNRM);
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fprintf(stderr, BRELN);
    }
}

void CZ::RFatal(const std::source_location location, const char *format, ...) noexcept
{
    if (level >= 1)
    {
        fprintf(stderr, "%sReam fatal:%s ", KRED, KNRM);
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fprintf(stderr, " %s %s(%d:%d) %s", location.function_name(), location.file_name(), location.line(), location.column(), BRELN);
    }
}

void CZ::RError(const char *format, ...) noexcept
{
    if (level >= 2)
    {
        fprintf(stderr, "%sReam error:%s ", KRED, KNRM);
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fprintf(stderr, BRELN);
    }
}

void CZ::RError(const std::source_location location, const char *format, ...) noexcept
{
    if (level >= 2)
    {
        fprintf(stderr, "%sReam error:%s ", KRED, KNRM);
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fprintf(stderr, " %s %s(%d:%d) %s", location.function_name(), location.file_name(), location.line(), location.column(), BRELN);
    }
}

void CZ::RWarning(const char *format, ...) noexcept
{
    if (level >= 3)
    {
        printf("%sReam warning:%s ", KYEL, KNRM);
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf(BRELN);
    }
}

void CZ::RWarning(const std::source_location location, const char *format, ...) noexcept
{
    if (level >= 3)
    {
        printf("%sReam warning:%s ", KYEL, KNRM);
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        fprintf(stderr, " %s %s(%d:%d) %s", location.function_name(), location.file_name(), location.line(), location.column(), BRELN);
    }
}

void CZ::RDebug(const char *format, ...) noexcept
{
    if (level >= 4)
    {
        printf("%sReam debug:%s ", KGRN, KNRM);
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf(BRELN);
    }
}

void CZ::RDebug(const std::source_location location, const char *format, ...) noexcept
{
    if (level >= 4)
    {
        printf("%sReam debug:%s ", KGRN, KNRM);
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        fprintf(stderr, " %s %s(%d:%d) %s", location.function_name(), location.file_name(), location.line(), location.column(), BRELN);
    }
}

void CZ::RLog(const char *format, ...) noexcept
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf(BRELN);
}

int CZ::RLogLevel() noexcept
{
    return level;
}

int CZ::REGLLogLevel() noexcept
{
    return eglLevel;
}

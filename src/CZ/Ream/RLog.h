#ifndef RLOG_H
#define RLOG_H

#include <CZ/CZ.h>
#include <source_location>

#if DOXYGEN
#define FORMAT_CHECK
#define FORMAT_CHECK2
#else
#define FORMAT_CHECK __attribute__((format(printf, 1, 2)))
#define FORMAT_CHECK2 __attribute__((format(printf, 2, 3)))
#endif

namespace CZ
{
    void RLogInit() noexcept;
    int RLogLevel() noexcept;
    int REGLLogLevel() noexcept;

    /// Prints general messages independent of the value of **REAM_DEBUG**.
    FORMAT_CHECK void RLog(const char *format, ...) noexcept;

    /// Reports an unrecoverable error. **REAM_DEBUG** >= 1.
    FORMAT_CHECK void RFatal(const char *format, ...) noexcept;
    FORMAT_CHECK2 void RFatal(const std::source_location location, const char *format, ...) noexcept;

    /// Reports a nonfatal error. **REAM_DEBUG** >= 2.
    FORMAT_CHECK void RError(const char *format, ...) noexcept;
    FORMAT_CHECK2 void RError(const std::source_location location, const char *format, ...) noexcept;

    /// Messages that report a risk for the compositor. **REAM_DEBUG** >= 3.
    FORMAT_CHECK void RWarning(const char *format, ...) noexcept;
    FORMAT_CHECK2 void RWarning(const std::source_location location, const char *format, ...) noexcept;

    /// Debugging messages. **REAM_DEBUG** >= 4.
    FORMAT_CHECK void RDebug(const char *format, ...) noexcept;
    FORMAT_CHECK2 void RDebug(const std::source_location location, const char *format, ...) noexcept;
}

#endif // RLOG_H

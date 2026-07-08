#ifndef RLOG_H
#define RLOG_H

#include <CZ/Core/CZLogger.h>

/**
 * @brief Convenience macro that resolves to Ream's shared logger instance.
 */
#define RLog RLogger()

/**
 * @brief Returns Ream's shared logger.
 *
 * The logger is created lazily on first use and is named "Ream". Its verbosity is
 * controlled by the `CZ_REAM_LOG_LEVEL` environment variable.
 *
 * @return A reference to the process-wide Ream logger.
 */
const CZ::CZLogger &RLogger() noexcept;

#endif // RLOG_H

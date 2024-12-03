/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_LOG_LOGMACROS_HXX
#define EPA_LIBRARY_LOG_LOGMACROS_HXX


/**
 * Note:
 * This file can be included multiple times in the same compilation unit in order to allow tests
 * to evaluate macros with different settings of preprocessor values. In production code this
 * file is included at most once in every compilation unit.
 */

/**
 * The implementation borrows many ideas from GLOG's logging.h. This includes
 * - forwarding LOG(s) to COMPACT_LOG##s macros (obviously the macro names in this file
 *   are not prefixed with GOOGLE).
 * - the EPA_LOG_IF macro is very similar to LOG_IF
 * - the individual definitions of COMPACT_LOG_* macros
 *
 * The main differences are
 * - there are new log levels INFO1 - INFO4, DEBUG0 - DEBUG4
 * - VLOG(#) is mapped to LOG(DEBUG#) instead of LOG(INFO)
 * - EPA_LOG_IF appends the call to `stream()`. `LOG_IF` does not do that and leaves that to the
 *   `LOG` macro. Appending `stream()` makes the forwarding of VLOG a little easier. It also makes
 *   the `EPA_LOG_IF` macro easier to understand.
 * - The `LOG_IF` trick is also used, in the form of the `NULL_STREAM` macro. `NULL_STREAM` is used
 *   where GLOG would use `google::NullStream` because the former prevents arguments to
 *   `operator<<` from being executed (and then discarded). That is not the case for
 *   `google::NullStream`. See GoogleLogTest.cxx for more details.
 * - The definitions of COMPACT_LOG_# macros use more and different predicates to decide which
 *   log levels/severities to display and which to hide.
 */


/**
 * Replace the definitions of the LOG and VLOG macros.  These are the only developer facing macros
 * in logging.h that are used in this repository.
 *
 * That means that macros like LOG_ASSERT or LOG_STRING or DLOG are not redefined and, when used,
 * will use GLOG directly. I.e. no JSON support etc.
 */

namespace epa
{

#undef LOG
#define LOG(severity) COMPACT_LOG_##severity

#undef LOG_INFO
#define LOG_INFO LOG(INFO)


#undef VLOG
#if defined ENABLE_DEBUG_LOG
#define VLOG(level)                                                                                \
    EPA_LOG_IF(                                                                                    \
        ::epa::Log::getLeveledSeverity(::epa::Log::Severity::DEBUG0, level),                       \
        IS_LOGLEVEL_SEVERITY_ON(::epa::Log::Severity::DEBUG0, level))
#else
#define VLOG(level) NULL_STREAM
#endif


/**
 * VLOG_IF is very much like VLOG, just with an additional runtime
 * condition.
 */
#undef VLOG_IF
#if defined ENABLE_DEBUG_LOG
#define VLOG_IF(level, condition)                                                                  \
    EPA_LOG_IF(                                                                                    \
        ::epa::Log::getLeveledSeverity(::epa::Log::Severity::DEBUG0, level),                       \
        (condition) && IS_LOGLEVEL_SEVERITY_ON(::epa::Log::Severity::DEBUG0, level))
#else
#define VLOG_IF(level, condition) NULL_STREAM
#endif


/**
 * Definition of macros for trace logs.
 *
 * TLOG is similar to LOG but does not show the message immediately but only when there is
 * an error.
 * TLOGV is like TLOG except that it does not append the `Log::Severity::` prefix and therefore
 * can be used for variables of type `Log::Severity` or fully qualified literals.
 *
 * SHOW_LOG_TRACE triggers the display of all deferred log messages. Typically called from
 * an exception handler.
 *
 * TLOG_FUNCTION_GUARD creates a RAII object that
 * a) writes log messages at entry and exit of the enclosing block, where the message
 *    includes the name of the enclosing function
 * b) call SHOW_LOG_TRACE when there is an uncaught exception at the time the destructor is
 *    executed.
 *
 * Note that trace log messages are collected per ServerSessionContext. When there is no
 * active ServerSessionContext, i.e. outside of a request handler, then TLOG messages are
 * displayed immediately.
 *
 * Note also that there is no FATAL severity for TLOG as that would abort the application
 * and no deferred log message would be shown.
 */

#undef TLOG
#define TLOG(severity) TLOGV(::epa::Log::Severity::severity)

#undef TLOGV
#define TLOGV(severity) EPA_TRACE_LOG_IF(severity, IS_SEVERITY_ON(severity))

#undef SHOW_LOG_TRACE
#define SHOW_LOG_TRACE() LogTrace::showLogTrace(here())

#undef TLOG_IF
#define TLOG_IF(severity, condition)                                                               \
    EPA_TRACE_LOG_IF(                                                                              \
        ::epa::Log::Severity::severity,                                                            \
        (condition) && IS_SEVERITY_ON(::epa::Log::Severity::severity))


/**
 * All following macros in this file are not supposed to be called from source code directly.
 * If you find an exception to this rule, please move the definition to somewhere above.
 */

#define EPA_LOG_IF(severity, condition)                                                            \
    static_cast<void>(0), ! (condition) ? (void) 0                                                 \
                                        : google::logging::internal::LogMessageVoidify()           \
                                              & ::epa::Log(severity, epa::here()).stream()

// Similar to EPA_LOG_IF but with a fixed condition that always selects the `(void)0` branch.
#define NULL_STREAM                                                                                \
    static_cast<void>(0),                                                                          \
        true ? (void) 0                                                                            \
             : google::logging::internal::LogMessageVoidify() & google::NullStream().stream()

/**
 * Evaluates to true when the value of environment variable `LOG_LEVEL` is equal to
 * or smaller than the given `severity`.
 * Examples:
 * LOG_IS_ON(INFO1) returns true for LOG_LEVEL==INFO1, LOG_LEVEL==INFO4 but not for LOG_LEVEL=WARNING.
 */
#define LOG_IS_ON(severity) (::epa::Log::getLogLevel(severity) >= ::epa::Log::logLevel.getValue())

/**
 * Evaluate if a log message at the given severity should be displayed.
 * In contrast to `LOG_IS_ON`, it also takes `GOOGLE_STRIP_LOG` into account.
 */
#define IS_SEVERITY_ON(severity)                                                                   \
    ((::epa::Log::getLogLevel(severity) >= GOOGLE_STRIP_LOG)                                       \
     && (::epa::Log::getLogLevel(severity) >= ::epa::Log::logLevel.getValue()))


/**
 * Definition of the COMPACT_LOG_DEBUG* macros.
 * Note that severity DEBUG is mapped to DEBUG0
 *
 * A DEBUG# level is shown if
 * - ENABLE_DEBUG_LOG is defined.
 * - GOOGLE_STRIP_LOG is <= #
 * - LOG_LEVEL is <= #
 */

// GEMREQ-start A_21195
#undef COMPACT_LOG_DEBUG0
#undef COMPACT_LOG_DEBUG
#if GOOGLE_STRIP_LOG <= 0 && defined ENABLE_DEBUG_LOG
#define COMPACT_LOG_DEBUG0                                                                         \
    EPA_LOG_IF(::epa::Log::Severity::DEBUG0, LOG_IS_ON(::epa::Log::Severity::DEBUG0))
#else
#define COMPACT_LOG_DEBUG0 NULL_STREAM
#define COMPACT_LOG_DEBUG NULL_STREAM
#endif

#undef COMPACT_LOG_DEBUG1
#if GOOGLE_STRIP_LOG <= -1 && defined ENABLE_DEBUG_LOG
#define COMPACT_LOG_DEBUG1                                                                         \
    EPA_LOG_IF(::epa::Log::Severity::DEBUG1, LOG_IS_ON(::epa::Log::Severity::DEBUG1))
#else
#define COMPACT_LOG_DEBUG1 NULL_STREAM
#endif

#undef COMPACT_LOG_DEBUG2
#if GOOGLE_STRIP_LOG <= -2 && defined ENABLE_DEBUG_LOG
#define COMPACT_LOG_DEBUG2                                                                         \
    EPA_LOG_IF(::epa::Log::Severity::DEBUG2, LOG_IS_ON(::epa::Log::Severity::DEBUG2))
#else
#define COMPACT_LOG_DEBUG2 NULL_STREAM
#endif

#undef COMPACT_LOG_DEBUG3
#if GOOGLE_STRIP_LOG <= -3 && defined ENABLE_DEBUG_LOG
#define COMPACT_LOG_DEBUG3                                                                         \
    EPA_LOG_IF(::epa::Log::Severity::DEBUG3, LOG_IS_ON(::epa::Log::Severity::DEBUG3))
#else
#define COMPACT_LOG_DEBUG3 NULL_STREAM
#endif

#undef COMPACT_LOG_DEBUG4
#if GOOGLE_STRIP_LOG <= -4 && defined ENABLE_DEBUG_LOG
#define COMPACT_LOG_DEBUG4                                                                         \
    EPA_LOG_IF(::epa::Log::Severity::DEBUG4, LOG_IS_ON(::epa::Log::Severity::DEBUG4))
#else
#define COMPACT_LOG_DEBUG4 NULL_STREAM
#endif
// GEMREQ-end A_21195


/**
 * Definition of the COMPACT_LOG_INFO* macros.
 * Note that INFO is mapped to INFO0
 *
 * An INFO# level is shown if
 * - GOOGLE_STRIP_LOG is <= #
 * - LOG_LEVEL is <= #
 */

#undef COMPACT_LOG_INFO0
#undef COMPACT_LOG_INFO
#if GOOGLE_STRIP_LOG <= 0
#define COMPACT_LOG_INFO0                                                                          \
    EPA_LOG_IF(::epa::Log::Severity::INFO0, LOG_IS_ON(::epa::Log::Severity::INFO0))
#define COMPACT_LOG_INFO                                                                           \
    EPA_LOG_IF(::epa::Log::Severity::INFO, LOG_IS_ON(::epa::Log::Severity::INFO0))
#else
#define COMPACT_LOG_INFO0 NULL_STREAM
#define COMPACT_LOG_INFO NULL_STREAM
#endif

#undef COMPACT_LOG_INFO1
#if GOOGLE_STRIP_LOG <= -1
#define COMPACT_LOG_INFO1                                                                          \
    EPA_LOG_IF(::epa::Log::Severity::INFO1, LOG_IS_ON(::epa::Log::Severity::INFO1))
#else
#define COMPACT_LOG_INFO1 NULL_STREAM
#endif

#undef COMPACT_LOG_INFO2
#if GOOGLE_STRIP_LOG <= -2
#define COMPACT_LOG_INFO2                                                                          \
    EPA_LOG_IF(::epa::Log::Severity::INFO2, LOG_IS_ON(::epa::Log::Severity::INFO2))
#else
#define COMPACT_LOG_INFO2 NULL_STREAM
#endif

#undef COMPACT_LOG_INFO3
#if GOOGLE_STRIP_LOG <= -3
#define COMPACT_LOG_INFO3                                                                          \
    EPA_LOG_IF(::epa::Log::Severity::INFO3, LOG_IS_ON(::epa::Log::Severity::INFO3))
#else
#define COMPACT_LOG_INFO3 NULL_STREAM
#endif

#undef COMPACT_LOG_INFO4
#if GOOGLE_STRIP_LOG <= -4
#define COMPACT_LOG_INFO4                                                                          \
    EPA_LOG_IF(::epa::Log::Severity::INFO4, LOG_IS_ON(::epa::Log::Severity::INFO4))
#else
#define COMPACT_LOG_INFO4 NULL_STREAM
#endif


/**
 * WARNING, ERROR and FATAL log levels are redefined as well.
 *
 * The WARNING (=1), ERROR(=2) and FATAL(=3) levels are shown if
 * - GOOGLE_STRIP_LOG is <= #
 * - LOG_LEVEL is <= #
 */

#undef COMPACT_LOG_WARNING
#if GOOGLE_STRIP_LOG <= 1
#define COMPACT_LOG_WARNING                                                                        \
    EPA_LOG_IF(::epa::Log::Severity::WARNING, LOG_IS_ON(::epa::Log::Severity::WARNING))
#else
#define COMPACT_LOG_WARNING NULL_STREAM
#endif


#undef COMPACT_LOG_ERROR
#if GOOGLE_STRIP_LOG <= 2
#define COMPACT_LOG_ERROR                                                                          \
    EPA_LOG_IF(::epa::Log::Severity::ERROR, LOG_IS_ON(::epa::Log::Severity::ERROR))
#else
#define COMPACT_LOG_ERROR NULL_STREAM
#endif


#undef COMPACT_LOG_FATAL
#if GOOGLE_STRIP_LOG <= 3
#define COMPACT_LOG_FATAL COMPACT_GOOGLE_LOG_FATAL.stream()
#else
#define COMPACT_LOG_FATAL NULL_STREAM
#endif


#undef IS_LOGLEVEL_SEVERITY_ON
#define IS_LOGLEVEL_SEVERITY_ON(severity, level)                                                   \
    ((GOOGLE_STRIP_LOG) <= (-(level))                                                              \
     && (::epa::Log::logLevel.getValue()                                                           \
         <= ::epa::Log::getLogLevel(::epa::Log::getLeveledSeverity(severity, (level))))            \
     && (VLOG_IS_ON(level)))

/**
 * The internal macros used by TLOG.
 */
#define EPA_TRACE_LOG_IF(severity, condition)                                                      \
    static_cast<void>(0),                                                                          \
        ! (condition)                                                                              \
            ? (void) 0                                                                             \
            : google::logging::internal::LogMessageVoidify()                                       \
                  & ::epa::Log(severity, ::epa::here(), ::epa::Log::Mode::DEFERRED).stream()


/**
 * Not all GLOG macros have been mapped to the our local ::epa::Log class. Nor should all macros be mapped.
 *
 * That means there these kinds of macros.
 * - the ones that use our own ::epa::Log class have been handled above
 * - the ones that need no adaptation:
 *     LOG_EVERY_N
 *     LOG_STRING
 *     LOG_TO_SINK
 *     LOG_TO_STRING
 *     PLOG
 *     SYSLOG*
 *     SYSLOG_EVERY_N
 * - macros that should be mapped but haven't are undefined below so that they are not used
 *   by mistake, these are handled below.
 * - macros that are defined based on macros mentioned above
 * - macros that have bin missed. If you find one, add it below.
 */

#undef CHECK
#define CHECK static_assert(false, "not (yet) supported")
#undef CHECK_OP
#define CHECK_OP static_assert(false, "not (yet) supported")
#undef DLOG
#define DLOG static_assert(false, "not (yet) supported")
#undef DLOG_ASSERT
#define DLOG_ASSERT static_assert(false, "not (yet) supported")
#undef DLOG_EVERY_N
#define DLOG_EVERY_N static_assert(false, "not (yet) supported")
#undef DLOG_IF
#define DLOG_IF static_assert(false, "not (yet) supported")
#undef DLOG_IF_EVERY_N
#define DLOG_IF_EVERY_N static_assert(false, "not (yet) supported")
#undef DVLOG
#define DVLOG static_assert(false, "not (yet) supported")
#undef GOOGLE_LOG_DFATAL
#define GOOGLE_LOG_DFATAL static_assert(false, "not (yet) supported")
#undef GOOGLE_LOG_ERROR
#define GOOGLE_LOG_ERROR static_assert(false, "not (yet) supported")
#undef GOOGLE_LOG_FATAL
#define GOOGLE_LOG_FATAL static_assert(false, "not (yet) supported")
#undef GOOGLE_LOG_INFO
#define GOOGLE_LOG_INFO static_assert(false, "not (yet) supported")
#undef GOOGLE_LOG_WARNING
#define GOOGLE_LOG_WARNING static_assert(false, "not (yet) supported")
#undef LOG_ASSERT
#define LOG_ASSERT static_assert(false, "not (yet) supported")
#undef LOG_FIRST_N
#define LOG_FIRST_N static_assert(false, "not (yet) supported")
#undef LOG_IF_EVERY_N
#define LOG_IF_EVERY_N static_assert(false, "not (yet) supported")
#undef VLOG_EVERY_N
#define VLOG_EVERY_N static_assert(false, "not (yet) supported")
#undef VLOG_IF_EVERY_N
#define VLOG_IF_EVERY_N static_assert(false, "not (yet) supported")

} // namespace epa

#endif

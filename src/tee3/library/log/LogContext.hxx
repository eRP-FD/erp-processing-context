/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_LOG_LOGCONTEXT_HXX
#define EPA_LIBRARY_LOG_LOGCONTEXT_HXX

namespace epa
{
/**
 * With the help of a `LogContext` value, classified texts can determine whether to show or hide
 * the text.
 */
enum class LogContext
{
    /**
     * Text is added to a log message.
     * Such messages are used to e.g. admins but not e.g. a record owner.
     * That means that personal or sensitive text must be hidden.
     */
    Log,

    /**
     * Text is returned to the caller in the free-form part of a response (usually an error response).
     * That means that personal or sensitive can be included in the encrypted part of a response when
     * it is accessible to the caller.
     */
    Response,

    Unknown
};

} // namespace epa

#endif

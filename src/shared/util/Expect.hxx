/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

/// NOTE: This file must not have a Header-Guard
/// this allows re-introducing macro values after 3rdparty headers changed them

#include "Expect.txx"

#include "shared/model/ModelException.hxx"
#include "shared/network/message/HttpStatus.hxx"
#include "shared/util/ErpException.hxx"
#include "shared/util/ErpServiceException.hxx"
#include "shared/util/TLog.hxx"

#include <stdexcept>


// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#undef fileAndLine
#define fileAndLine FileNameAndLineNumber(__FILE__, __LINE__)

/**
 * Throw a runtime_exception if `expression` does not evaluate to true.
 * The given `message` is passed into the exception and also printed to the log, together with the file
 * name and line number so that the origin of the exception can be easily determined.
 */
#undef Expect
#define Expect(expression, message) \
    local::throwIfNot<std::runtime_error>(static_cast<bool>(expression), #expression, [&]{ return (message); }, fileAndLine)

#undef Fail
#define Fail(message) \
    local::logAndThrow<std::runtime_error>(message, fileAndLine)

#undef Fail2
#define Fail2(message, exception_class) \
    local::logAndThrow<exception_class>(message, fileAndLine)

/**
 * Variant of the Fail macro that throws logic error.
 */
#undef LogicErrorFail
#define LogicErrorFail(message) \
    local::logAndThrow<std::logic_error>(message, fileAndLine)

/**
 * Variant of the Expect macro that accepts an additional status value that will be returned with the HTTP response.
 */
#undef ErpExpect
#define ErpExpect(expression, errorStatus, message) \
    local::throwIfNot<ErpException>(static_cast<bool>(expression), #expression, [&]{ return (message); }, fileAndLine, errorStatus)
#undef ErpExpectWithDiagnostics
#define ErpExpectWithDiagnostics(expression, errorStatus, message, diagnostics) \
    local::throwIfNot<ErpException>(static_cast<bool>(expression), #expression, [&]{ return (message); }, fileAndLine, errorStatus, diagnostics)
#undef VauExpect
#define VauExpect(expression, errorStatus, vauError, message) \
    local::throwIfNot<ErpException>(static_cast<bool>(expression), #expression, [&]{ return (message); }, fileAndLine, errorStatus, vauError)

#undef ErpFail
#define ErpFail(errorStatus, message) \
    local::logAndThrow<ErpException>(message, fileAndLine, errorStatus)
#undef ErpFail2
#define ErpFail2(errorStatus, message, message2) \
    local::logAndThrow<ErpServiceException>(message, fileAndLine, errorStatus, message2)
#undef ErpFailWithDiagnostics
#define ErpFailWithDiagnostics(errorStatus, message, diagnostics) \
    local::logAndThrow<ErpException>(message, fileAndLine, errorStatus, diagnostics)
#undef VauFail
#define VauFail(errorStatus, vauError, message) \
    local::logAndThrow<ErpException>(message, fileAndLine, errorStatus, vauError)
#define VauFailWithDiagnostics(errorStatus, vauError, message, diagnostics) \
    local::logAndThrow<ErpException>(message, fileAndLine, errorStatus, diagnostics, vauError)

/**
 * Variant of the Expect macro that should be used for errors originating in model classes.
 */
#undef ModelExpect
#define ModelExpect(expression, message) \
    local::throwIfNot<::model::ModelException>(static_cast<bool>(expression), #expression, [&]{ return (message); }, fileAndLine)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ModelExpectNoThrow(expression, message)                                                                        \
    try                                                                                                                \
    {                                                                                                                  \
        (expression);                                                                                                  \
    }                                                                                                                  \
    catch (const std::exception& ex)                                                                                   \
    {                                                                                                                  \
        local::throwIfNot<::model::ModelException>(                                                                    \
            false, #expression,                                                                                        \
            [&] {                                                                                                      \
                return (std::string{message} + " // " + ex.what());                                                    \
            },                                                                                                         \
            fileAndLine);                                                                                              \
    }

#undef ModelFail
#define ModelFail(message) \
    local::logAndThrow<::model::ModelException>(message, fileAndLine)

/**
 * Variant of the Expect macro that lets you specify the class of the exception.
 * With C++ 20's improved support for VARARG macros and also its support for non-macro access to file name and line number
 * this macro (and the other macros in this file) could probably be improved considerably.
 */
#undef Expect3
#define Expect3(expression, message, exceptionClass) \
    local::throwIfNot<exceptionClass>(static_cast<bool>(expression), #expression, [&]{ return (message); }, fileAndLine)

/**
 * Variant of the Expect macro that collects the openssl errors and add it to the message.
 */
#undef OpenSslExpect
#define OpenSslExpect(expression, message) \
    local::throwIfNotWithOpenSslErrors<std::runtime_error>(static_cast<bool>(expression), #expression, [&]{ return (message); }, fileAndLine)

/**
 * Variant of the Expect macro that should be used for TSL related errors.
 */
#undef TslExpect
#define TslExpect(expression, message, errorCode) \
    local::throwIfNot<TslError>(static_cast<bool>(expression), #expression, [&]{ return (message); }, fileAndLine, errorCode)
#undef TslExpect4
#define TslExpect4(expression, message, errorCode, tslMode) \
    local::throwIfNot<TslError>(static_cast<bool>(expression), #expression, [&]{ return (message); }, fileAndLine, errorCode, tslMode)
#undef TslExpect6
#define TslExpect6(expression, message, errorCode, tslMode, tslId, tslSequenceNumber) \
    local::throwIfNot<TslError>(static_cast<bool>(expression), #expression, [&]{ return (message); }, fileAndLine, errorCode, tslMode, tslId, tslSequenceNumber)

#undef TslFail
#define TslFail(message, errorCode) \
    local::logAndThrow<TslError>(message, fileAndLine, errorCode)
#undef TslFailStack
#define TslFailStack(message, errorCode, tslError) \
    local::logAndThrow<TslError>(message, fileAndLine, errorCode, tslError)
#undef TslFail3
#define TslFail3(message, errorCode, tslMode) \
    local::logAndThrow<TslError>(message, fileAndLine, errorCode, tslMode)
#undef TslFail5
#define TslFail5(message, errorCode, tslMode, tslId, tslSequenceNumber) \
    local::logAndThrow<TslError>(message, fileAndLine, errorCode, tslMode, tslId, tslSequenceNumber)

#undef TslFailWithStatus
#define TslFailWithStatus(message, errorCode, httpStatus, tslMode) \
    local::logAndThrow<TslError>(message, fileAndLine, errorCode, httpStatus)

// NOLINTEND(cppcoreguidelines-macro-usage)

/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_UTIL_EXPECT_HXX
#define ERP_UTIL_EXPECT_HXX

#include "erp/util/ErpException.hxx"
#include "erp/util/ExceptionWrapper.hxx"
#include "erp/model/ModelException.hxx"

#include "erp/util/TLog.hxx"
#include "erp/common/HttpStatus.hxx"

#include <string>
#include <stdexcept>
#include <sstream>
#include <cstddef>
#include <openssl/err.h>

// to be intended to be used internal only, but due to templating it is needed in the header
namespace local
{

    template<class Exception, class ... Arguments>
    [[noreturn]]
    void logAndThrow (
        const std::string& message,
        const FileNameAndLineNumber& fileNameAndLineNumber,
        Arguments&& ... arguments)
    {
        TVLOG(1) << message << " at " << fileNameAndLineNumber.fileName << ':' << fileNameAndLineNumber.line;
        throw ExceptionWrapper<Exception>::create(fileNameAndLineNumber, message,
                                                  std::forward<Arguments>(arguments)...);
    }


    template<class Exception, class ... Arguments>
    void throwIfNot (
        const bool expectedTrue,
        const std::string& expression,
        const std::string& message,
        const FileNameAndLineNumber& fileNameAndLineNumber,
        Arguments&& ... arguments)
    {
        if (!expectedTrue)
        {
            TVLOG(1) << message << " at " << fileNameAndLineNumber.fileName << ':'
                    << fileNameAndLineNumber.line << "\n    " << expression
                    << " did not evaluate to true";
            throw ExceptionWrapper<Exception>::create(fileNameAndLineNumber, message,
                                                      std::forward<Arguments>(arguments)...);
        }
    }

    template<class Exception, class ... Arguments>
    void throwIfNotWithOpenSslErrors (
        const bool expectedTrue,
        const std::string& expression,
        const std::string& message,
        const FileNameAndLineNumber& fileNameAndLineNumber,
        Arguments&& ... arguments)
    {
        if (!expectedTrue)
        {
            // collect OpenSSL errors
            std::string errors;
            while (unsigned long error = ERR_get_error())
            {
                if (!errors.empty())
                {
                    errors += "; ";
                }

                errors += ERR_error_string(error, nullptr);
            }
            const std::string detailedMessage{message + " " + errors};
            TVLOG(1) << detailedMessage << " at " << fileNameAndLineNumber.fileName << ':'
                    << fileNameAndLineNumber.line << "\n    " << expression
                    << " did not evaluate to true";
            throw ExceptionWrapper<Exception>::create(fileNameAndLineNumber, detailedMessage,
                                                      std::forward<Arguments>(arguments)...);
        }
    }
}


#define fileAndLine FileNameAndLineNumber(__FILE__, __LINE__)


/**
 * Throw a runtime_exception if `expression` does not evaluate to true.
 * The given `message` is passed into the exception and also printed to the log, together with the file
 * name and line number so that the origin of the exception can be easily determined.
 */
#define Expect(expression, message) \
    local::throwIfNot<std::runtime_error>(static_cast<bool>(expression), #expression, message, fileAndLine)

#define Fail(message) \
    local::logAndThrow<std::runtime_error>(message, fileAndLine)

#define Fail2(message, exception_class) \
    local::logAndThrow<exception_class>(message, fileAndLine)

/**
 * Variant of the Fail macro that throws logic error.
 */
#define LogicErrorFail(message) \
    local::logAndThrow<std::logic_error>(message, fileAndLine)

/**
 * Variant of the Expect macro that accepts an additional status value that will be returned with the HTTP response.
 */
#define ErpExpect(expression, errorStatus, message) \
    local::throwIfNot<ErpException>(static_cast<bool>(expression), #expression, message, fileAndLine, errorStatus)
#define ErpExpectWithDiagnostics(expression, errorStatus, message, diagnostics) \
    local::throwIfNot<ErpException>(static_cast<bool>(expression), #expression, message, fileAndLine, errorStatus, diagnostics)
#define VauExpect(expression, errorStatus, vauError, message) \
    local::throwIfNot<ErpException>(static_cast<bool>(expression), #expression, message, fileAndLine, errorStatus, vauError)

#define ErpFail(errorStatus, message) \
    local::logAndThrow<ErpException>(message, fileAndLine, errorStatus)
#define ErpFailWithDiagnostics(errorStatus, message, diagnostics) \
    local::logAndThrow<ErpException>(message, fileAndLine, errorStatus, diagnostics)
#define VauFail(errorStatus, vauError, message) \
    local::logAndThrow<ErpException>(message, fileAndLine, errorStatus, vauError)
#define VauFailWithDiagnostics(errorStatus, vauError, message, diagnostics) \
    local::logAndThrow<ErpException>(message, fileAndLine, errorStatus, diganostics, vauError)

/**
 * Variant of the Expect macro that should be used for errors originating in model classes.
 */
#define ModelExpect(expression, message) \
    local::throwIfNot<::model::ModelException>(static_cast<bool>(expression), #expression, message, fileAndLine)

#define ModelFail(message) \
    local::logAndThrow<::model::ModelException>(message, fileAndLine)

/**
 * Variant of the Expect macro that lets you specify the class of the exception.
 * With C++ 20's improved support for VARARG macros and also its support for non-macro access to file name and line number
 * this macro (and the other macros in this file) could probably be improved considerably.
 */
#define Expect3(expression, message, exceptionClass) \
    local::throwIfNot<exceptionClass>(static_cast<bool>(expression), #expression, message, fileAndLine)

/**
 * Variant of the Expect macro that collects the openssl errors and add it to the message.
 */
#define OpenSslExpect(expression, message) \
    local::throwIfNotWithOpenSslErrors<std::runtime_error>(static_cast<bool>(expression), #expression, message, fileAndLine)

/**
 * Variant of the Expect macro that should be used for TSL related errors.
 */
#define TslExpect(expression, message, errorCode) \
    local::throwIfNot<TslError>(static_cast<bool>(expression), #expression, message, fileAndLine, errorCode)
#define TslExpect4(expression, message, errorCode, tslMode) \
    local::throwIfNot<TslError>(static_cast<bool>(expression), #expression, message, fileAndLine, errorCode, tslMode)
#define TslExpect6(expression, message, errorCode, tslMode, tslId, tslSequenceNumber) \
    local::throwIfNot<TslError>(static_cast<bool>(expression), #expression, message, fileAndLine, errorCode, tslMode, tslId, tslSequenceNumber)

#define TslFail(message, errorCode) \
    local::logAndThrow<TslError>(message, fileAndLine, errorCode)
#define TslFailStack(message, errorCode, tslError) \
    local::logAndThrow<TslError>(message, fileAndLine, errorCode, tslError)
#define TslFail3(message, errorCode, tslMode) \
    local::logAndThrow<TslError>(message, fileAndLine, errorCode, tslMode)
#define TslFail5(message, errorCode, tslMode, tslId, tslSequenceNumber) \
    local::logAndThrow<TslError>(message, fileAndLine, errorCode, tslMode, tslId, tslSequenceNumber)

#define TslFailWithStatus(message, errorCode, httpStatus, tslMode) \
    local::logAndThrow<TslError>(message, fileAndLine, errorCode, httpStatus)
#endif

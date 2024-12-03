/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TSL_TSLPARSINGERROR_HXX
#define ERP_PROCESSING_CONTEXT_TSL_TSLPARSINGERROR_HXX

#include "shared/tsl/error/TslErrorCode.hxx"
#include "shared/tsl/TslMode.hxx"
#include "shared/network/message/HttpStatus.hxx"

#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

class TslError : public std::runtime_error
{
public:
    class ErrorData
    {
    public:
        std::string message;
        TslErrorCode errorCode;
    };

    TslError(
        std::string message,
        TslErrorCode errorCode);

    TslError(
        std::string message,
        TslErrorCode errorCode,
        HttpStatus httpStatus);

    TslError(
        std::string message,
        TslErrorCode errorCode,
        const TslMode tslMode);

    TslError(
        std::string message,
        TslErrorCode errorCode,
        const TslMode tslMode,
        std::optional<std::string> tslId,
        std::string tslSequenceNumber);

    TslError(
        std::string message,
        TslErrorCode errorCode,
        const TslError& previous);

    TslError(
        const TslError& original,
        HttpStatus newHttpStatus);

    ~TslError() override = default;

    const std::vector<ErrorData>& getErrorData() const;
    const std::optional<TslMode>& getTslMode() const;
    const std::optional<std::string>& getTslId() const;
    const std::optional<std::string>& getTslSequenceNumber() const;

    HttpStatus getHttpStatus() const;

private:
    const std::vector<ErrorData> mErrorData;
    const std::optional<TslMode> mTslMode;
    const std::optional<std::string> mTslId;
    const std::optional<std::string> mTslSequenceNumber;

    const HttpStatus mStatus;
};


template <class Exception, typename Tag>
class WrappedErrorClass : public Exception
{
public:
    WrappedErrorClass(const std::string& message)
    : Exception(message)
    {}
};

using CryptoFormalError = WrappedErrorClass<std::runtime_error, struct CryptoFormalErrorTag>;

#endif

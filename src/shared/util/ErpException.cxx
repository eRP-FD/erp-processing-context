/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/ErpException.hxx"


ErpException::ErpException(const std::string& message, const HttpStatus status)
    : ErpException(message, status, {}, {})
{
}

ErpException::ErpException(const std::string& message, const HttpStatus status, const std::optional<std::string>& diagnostics)
        : ErpException(message, status, diagnostics, {})
{
}

ErpException::ErpException(const std::string& message, const HttpStatus status,
                           const std::optional<VauErrorCode> vauErrorCode)
    : ErpException(message, status, {}, vauErrorCode)
{
}

ErpException::ErpException(const std::string& message, const HttpStatus status,
                           const std::optional<std::string>& diagnostics, const std::optional<VauErrorCode> vauErrorCode)
    : std::runtime_error(message), mStatus(status), mDiagnostics(diagnostics), mVauErrorCode(vauErrorCode)
{
}

HttpStatus ErpException::status(void) const
{
    return mStatus;
}

const std::optional<std::string>& ErpException::diagnostics() const
{
    return mDiagnostics;
}

const std::optional<VauErrorCode>& ErpException::vauErrorCode() const
{
    return mVauErrorCode;
}

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ERPEXCEPTION_HXX
#define ERP_PROCESSING_CONTEXT_ERPEXCEPTION_HXX

#include "erp/common/HttpStatus.hxx"
#include "erp/common/VauErrorCode.hxx"

#include <stdexcept>


class ErpException : public std::runtime_error
{
public:
    ErpException(const std::string& message, const HttpStatus status);
    ErpException(const std::string& message, const HttpStatus status, const std::optional<std::string>& diagnostics);
    ErpException(const std::string& message, const HttpStatus status, const std::optional<VauErrorCode> vauErrorCode);
    ErpException(const std::string& message, const HttpStatus status, const std::optional<std::string>& diagnostics, const std::optional<VauErrorCode> vauErrorCode);

    HttpStatus status(void) const;
    const std::optional<std::string>& diagnostics() const;
    const std::optional<VauErrorCode>& vauErrorCode() const;

    ErpException(const ErpException&) = default;
    ErpException(ErpException&&) noexcept = default;
    ErpException& operator = (const ErpException&) = default;
    ErpException& operator = (ErpException&&) noexcept = default;
private:
    HttpStatus mStatus;
    std::optional<std::string> mDiagnostics;
    std::optional<VauErrorCode> mVauErrorCode;
};


#endif

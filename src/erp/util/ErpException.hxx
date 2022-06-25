/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
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

private:
    HttpStatus mStatus;
    std::optional<std::string> mDiagnostics;
    std::optional<VauErrorCode> mVauErrorCode;
};


#endif

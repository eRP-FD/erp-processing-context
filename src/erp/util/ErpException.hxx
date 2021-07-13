#ifndef ERP_PROCESSING_CONTEXT_ERPEXCEPTION_HXX
#define ERP_PROCESSING_CONTEXT_ERPEXCEPTION_HXX

#include "erp/common/HttpStatus.hxx"
#include "erp/common/VauErrorCode.hxx"

#include <stdexcept>


class ErpException : public std::runtime_error
{
public:
    ErpException(const std::string& message, HttpStatus status);
    ErpException(const std::string& message, HttpStatus status, std::optional<VauErrorCode> vauErrorCode);
    HttpStatus status(void) const;
    const std::optional<VauErrorCode>& vauErrorCode() const;

private:
    HttpStatus mStatus;
    std::optional<VauErrorCode> mVauErrorCode;
};


#endif

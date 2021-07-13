#include "erp/util/ErpException.hxx"


ErpException::ErpException(const std::string& message, const HttpStatus status)
    : ErpException(message, status, {})
{
}

ErpException::ErpException(const std::string& message, const HttpStatus status,
                           const std::optional<VauErrorCode> vauErrorCode)
    : std::runtime_error(message), mStatus(status), mVauErrorCode(vauErrorCode)
{
}

HttpStatus ErpException::status(void) const
{
    return mStatus;
}

const std::optional<VauErrorCode>& ErpException::vauErrorCode() const
{
    return mVauErrorCode;
}

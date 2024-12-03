/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ERPSERVICEEXCEPTION_HXX
#define ERP_PROCESSING_CONTEXT_ERPSERVICEEXCEPTION_HXX

#include "shared/network/message/HttpStatus.hxx"
#include "shared/common/VauErrorCode.hxx"

#include <memory>
#include <stdexcept>

namespace model
{
class OperationOutcome;
}

class ErpServiceException : public std::runtime_error
{
public:
    ErpServiceException(const std::string& message, HttpStatus status, const std::string& message2);
    ~ErpServiceException() override;

    [[nodiscard]] HttpStatus status() const;
    [[nodiscard]] const model::OperationOutcome& operationOutcome() const;
    [[nodiscard]] const std::optional<VauErrorCode>& vauErrorCode() const;

private:
    HttpStatus mStatus;
    std::unique_ptr<model::OperationOutcome> mOperationOutcome;
    std::optional<VauErrorCode> mVauErrorCode;
};


#endif //ERP_PROCESSING_CONTEXT_ERPSERVICEEXCEPTION_HXX

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/ErpServiceException.hxx"
#include "erp/model/OperationOutcome.hxx"

ErpServiceException::ErpServiceException(const std::string& message, HttpStatus status, const std::string& message2)
    : std::runtime_error(message)
      , mStatus(status)
      , mOperationOutcome(std::make_unique<model::OperationOutcome>(model::OperationOutcome::Issue{
          .severity = model::OperationOutcome::Issue::Severity::error,
          .code = model::OperationOutcome::httpCodeToOutcomeIssueType(status),
          .detailsText = message, .diagnostics = {}, .expression = {}}))
{
    mOperationOutcome->addIssue({.severity = model::OperationOutcome::Issue::Severity::error,
                                .code = model::OperationOutcome::httpCodeToOutcomeIssueType(status),
                                .detailsText = message2, .diagnostics = {}, .expression = {}});
}

ErpServiceException::~ErpServiceException() = default;

HttpStatus ErpServiceException::status() const
{
    return mStatus;
}

const model::OperationOutcome& ErpServiceException::operationOutcome() const
{
    return *mOperationOutcome;
}

const std::optional<VauErrorCode>& ErpServiceException::vauErrorCode() const
{
    return mVauErrorCode;
}

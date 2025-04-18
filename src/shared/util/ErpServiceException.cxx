/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/ErpServiceException.hxx"
#include "shared/model/Coding.hxx"
#include "shared/model/OperationOutcome.hxx"

ErpServiceException::ErpServiceException(const std::string& message, HttpStatus status, const std::string& message2)
    : std::runtime_error(message)
    , mStatus(status)
    , mOperationOutcome(std::make_unique<model::OperationOutcome>(
          model::OperationOutcome::Issue{.severity = model::OperationOutcome::Issue::Severity::error,
                                         .code = model::OperationOutcome::httpCodeToOutcomeIssueType(status),
                                         .detailsText = message,
                                         .detailsCodings = {},
                                         .diagnostics = {},
                                         .expression = {}}))
{
    mOperationOutcome->addIssue({.severity = model::OperationOutcome::Issue::Severity::error,
                                 .code = model::OperationOutcome::httpCodeToOutcomeIssueType(status),
                                 .detailsText = message2,
                                 .detailsCodings = {},
                                 .diagnostics = {},
                                 .expression = {}});
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

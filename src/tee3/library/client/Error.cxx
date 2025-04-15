/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/client/Error.hxx"
#include "library/log/Logging.hxx"

#include <sstream>

namespace epa
{

std::string FailedOperation::toString() const
{
    std::ostringstream oss;
    oss << "fqdn = " << fqdn;
    oss << ", operation = " << operation;
    if (httpStatus.has_value())
        oss << ", httpStatus = " << httpStatus.value();
    if (errorCode.has_value())
        oss << ", errorCode = " << errorCode.value();
    return oss.str();
}


BaseError::BaseError(
    const Location& location,
    const std::string& what,
    std::optional<FailedOperation> failedOperation,
    ErrorImpact impact)
  : std::runtime_error(what),
    mProgramLocationCode(location.calculateProgramLocationCode()),
    mFailedOperation(std::move(failedOperation)),
    mImpact(impact)
{
    VLOG(2) << "BaseError constructed with: "
            << "programLocationCode = " << mProgramLocationCode << ", what = \"" << what
            << "\", failedOperation = {"
            << (mFailedOperation.has_value() ? mFailedOperation.value().toString() : "") << "}";
}


int32_t BaseError::programLocationCode() const
{
    return mProgramLocationCode;
}


const std::optional<FailedOperation>& BaseError::failedOperation() const
{
    return mFailedOperation;
}


void BaseError::setFailedOperation(FailedOperation failedOperation)
{
    mFailedOperation = std::move(failedOperation);
}


bool BaseError::shouldTerminateSession() const
{
    return mImpact == ErrorImpact::TerminateSession
           || mImpact == ErrorImpact::TerminateSessionAndRetry;
}


bool BaseError::shouldRetryOperation() const
{
    return mImpact == ErrorImpact::TerminateSessionAndRetry;
}


DeviceConfirmationError::DeviceConfirmationError(
    const Location& location,
    const std::string& what,
    FailedOperation failedOperation,
    int remainingAttempts)
  : CommunicationError(location, what, std::move(failedOperation)),
    remainingAttempts(remainingAttempts)
{
}


DeviceRegistrationError::DeviceRegistrationError(
    const Location& location,
    const std::string& what,
    FailedOperation failedOperation,
    SystemTime endOfWaitingTime)
  : CommunicationError(location, what, std::move(failedOperation)),
    endOfWaitingTime(endOfWaitingTime)
{
}

} // namespace epa

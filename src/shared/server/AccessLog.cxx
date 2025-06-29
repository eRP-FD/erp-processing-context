/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/server/AccessLog.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/util/ExceptionHelper.hxx"
#include "shared/util/TLog.hxx"


AccessLog::AccessLog (void)
    : mStartTime(std::chrono::system_clock::now()),
      mLog(LogId::ACCESS, JsonLog::makeInfoLogReceiver(), false)
{
    mLog.keyValue(
        "log_type",
        "access");
    mLog.keyValue(
        "timestamp",
        model::Timestamp(mStartTime.value()).toXsDateTime());
}


AccessLog::AccessLog (std::ostream& os)
    : mStartTime(std::chrono::system_clock::now()),
      mLog(LogId::ACCESS, os, false)
{
    mLog.keyValue(
        "log_type",
        "access");
    mLog.keyValue(
        "timestamp",
        model::Timestamp(mStartTime.value()).toXsDateTime());
}


AccessLog::~AccessLog (void)
{
    markEnd();
}


void AccessLog::markEnd (void)
{
    const auto endTime = std::chrono::system_clock::now();

    if ( ! mError.empty())
        mLog.keyValue(
            "error",
            mError);

    if (mStartTime.has_value())
    {
        mLog.keyValue(
            "response_time",
            static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endTime - mStartTime.value()).count())/1000.0);
        mStartTime = std::nullopt;
    }
}

void AccessLog::location(const FileNameAndLineNumber& loc)
{
    mLog.location(loc);
}

void AccessLog::locationFromException(const std::exception& ex)
{
    mLog.locationFromException(ex);
}

void AccessLog::locationFromException(const boost::exception& ex)
{
    mLog.locationFromException(ex);
}

void AccessLog::setInnerRequestOperation (const std::string_view& operation)
{
    mLog.keyValue(
        "inner_request_operation",
        operation);

    TVLOG(1) << "starting to process " << operation;
}


void AccessLog::updateFromOuterRequest (const ServerRequest& request)
{
    const auto optionalRequestId = request.header().header(Header::XRequestId);
    if(optionalRequestId)
    {
        const auto RequestId = String::removeEnclosing("[", "]", optionalRequestId.value());
        mLog.keyValue("x_request_id", RequestId);
    }
    else
    {
        mLog.keyValue("x_request_id", "<not-set>");
    }
}


void AccessLog::updateFromInnerResponse (const ServerResponse& response)
{
    mLog.keyValue(
        "inner_response_code",
        static_cast<size_t>(response.getHeader().status()));
}


void AccessLog::updateFromOuterResponse (const ServerResponse& response)
{
    mLog.keyValue(
        "response_code",
        static_cast<size_t>(response.getHeader().status()));
}


void AccessLog::message (const std::string_view message)
{
    mLog.message(message);
}


void AccessLog::error (const std::string_view message)
{
    if ( ! mError.empty())
        mError += ";";
    mError += message;
}


void AccessLog::error (const std::string_view message, std::exception_ptr exception)
{
    ExceptionHelper::extractInformation(
        [this, message]
        (const std::string& details, const std::string& location)
        {
            keyValue("location", location);
            error(std::string(message) + ": " + details);
        },
        std::move(exception));
}


void AccessLog::discard (void)
{
    mLog.discard();
}


void AccessLog::prescriptionId(const model::PrescriptionId& id)
{
    keyValue("prescription_id", id.toString());
    mPrescriptionId.emplace(id);
}

const std::optional<model::PrescriptionId>& AccessLog::getPrescriptionId() const
{
    return mPrescriptionId;
}

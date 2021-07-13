#include "erp/server/AccessLog.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/server/request/ServerRequest.hxx"

#include "erp/model/Timestamp.hxx"

AccessLog::AccessLog (void)
    : mStartTime(std::chrono::system_clock::now()),
      mLog(LogId::ACCESS, JsonLog::makeWarningLogReceiver(), false)
{
    mLog.keyValue(
        "log-type",
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
        "log-type",
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
            "response-time",
            static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endTime - mStartTime.value()).count())/1000.0,
            3);
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
        "inner-request-operation",
        operation);

    TVLOG(1) << "starting to process " << operation;
}


void AccessLog::updateFromOuterRequest (const ServerRequest& request)
{
    const auto optionalRequestId = request.header().header("X-Request-Id");
    mLog.keyValue(
        "x-request-id",
        optionalRequestId.value_or("<not-set>"));
}


void AccessLog::updateFromInnerRequest (const ServerRequest& request)
{
    (void)request;
}


void AccessLog::updateFromInnerResponse (const ServerResponse& response)
{
    mLog.keyValue(
        "inner-response-code",
        static_cast<size_t>(response.getHeader().status()));
}


void AccessLog::updateFromOuterResponse (const ServerResponse& response)
{
    mLog.keyValue(
        "response-code",
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


void AccessLog::discard (void)
{
    mLog.discard();
}

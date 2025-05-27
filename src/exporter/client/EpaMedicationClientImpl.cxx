/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/client/EpaMedicationClientImpl.hxx"
#include "exporter/network/client/Tee3ClientPool.hxx"
#include "exporter/network/client/Tee3ClientsForHost.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/Uuid.hxx"
#include "shared/util/Configuration.hxx"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_future.hpp>
#include <rapidjson/document.h>
#include <future>

namespace {

// GEMREQ-start A_24773#restart-graceful
bool isGracefulError(boost::system::error_code ec)
{
    return ec == Tee3Client::error::restart || ec == boost::asio::ssl::error::stream_truncated ||
           ec == boost::beast::http::error::end_of_stream || ec == boost::asio::error::connection_reset ||
           ec == boost::asio::error::not_connected;
}
// GEMREQ-end A_24773#restart-graceful

} // namespace


EpaMedicationClientImpl::EpaMedicationClientImpl(boost::asio::io_context& ioContext, std::string hostname,
                                                 std::shared_ptr<Tee3ClientPool> teeClientPool)

    : mIoContext{ioContext}
    , mHostname{std::move(hostname)}
    , mTeeClientPool{std::move(teeClientPool)}
    , mStickyClient(nullptr, Tee3ClientsForHost::Tee3ClientDeleter{mTeeClientPool->weak_from_this()})
    , mUseStickyClient(Configuration::instance().getBoolValue(ConfigurationKey::MEDICATION_EXPORTER_VAU_HTTPS_CLIENT_STICKY))
{
}

EpaMedicationClientImpl::Response EpaMedicationClientImpl::sendProvidePrescription(const std::string& xRequestId,
                                                                                   const model::Kvnr& kvnr,
                                                                                   const std::string& payload)
{
    using boost::beast::http::verb;
    auto req = request(verb::post, "/epa/medication/api/v1/fhir/$provide-prescription-erp", kvnr);
    req.body() = payload;
    return sendRequestBlocking(xRequestId, std::move(req));
}


EpaMedicationClientImpl::Response EpaMedicationClientImpl::sendProvideDispensation(const std::string& xRequestId,
                                                                                   const model::Kvnr& kvnr,
                                                                                   const std::string& payload)
{
    using boost::beast::http::verb;
    auto req = request(verb::post, "/epa/medication/api/v1/fhir/$provide-dispensation-erp", kvnr);
    req.body() = payload;
    return sendRequestBlocking(xRequestId, std::move(req));
}

EpaMedicationClientImpl::Response EpaMedicationClientImpl::sendCancelPrescription(const std::string& xRequestId,
                                                                                  const model::Kvnr& kvnr,
                                                                                  const std::string& payload)
{
    using boost::beast::http::verb;
    auto req = request(verb::post, "/epa/medication/api/v1/fhir/$cancel-prescription-erp", kvnr);
    req.body() = payload;
    return sendRequestBlocking(xRequestId, std::move(req));
}

EpaMedicationClientImpl::Response EpaMedicationClientImpl::sendRequestBlocking(const std::string& xRequestId,
                                                                               Tee3Client::Request req)
{
    std::future<Tee3Client::Response> fut = co_spawn(
        mIoContext,
        sendRequest(xRequestId, std::move(req)),
        boost::asio::use_future);
    auto response = fut.get();
    return Response{.httpStatus = fromBoostBeastStatus(static_cast<uint32_t>(response.result_int())),
                    .body = model::NumberAsStringParserDocument::fromJson(response.body())};
}

bool EpaMedicationClientImpl::testConnection()
{
    auto fut = co_spawn(
        mIoContext,
        // The lifetime of lambda capture is no issue here, because of fut.get() below, which waits for the coroutine.
        [this]() -> boost::asio::awaitable<bool> {// NOLINT(cppcoreguidelines-avoid-capturing-lambda-coroutines)
            try
            {
                auto tee3Client = co_await mTeeClientPool->acquire(mHostname);
                co_return tee3Client != nullptr;
            }
            catch (const std::exception& e)
            {
                co_return false;
            }
        },
        boost::asio::use_future);
    return fut.get();
}


boost::asio::awaitable<Tee3Client::Response> EpaMedicationClientImpl::sendRequest(std::string xRequestId,
                                                                                  Tee3Client::Request req)
{
    if (!mStickyClient)
    {
        mStickyClient = co_await mTeeClientPool->acquire(mHostname);
    }
    boost::system::result<Tee3Client::Response> resp;

    // GEMREQ-start A_24773#reconnect
    while (mStickyClient && isGracefulError((resp = co_await mStickyClient->sendInner(xRequestId, req, mLogDataBag)).error()))
    {
        mStickyClient.reset();
        mStickyClient = co_await mTeeClientPool->acquire(mHostname);
    }
    // GEMREQ-end A_24773#reconnect

    if (!mStickyClient)
    {
        resp = std::make_error_code(std::errc::resource_unavailable_try_again);
    }

    if (!mUseStickyClient)
    {
        mStickyClient.reset();
    }

    Expect(resp.has_value(), "Response had an error: " + resp.error().message());
    co_return resp.value();
}

Tee3Client::Request EpaMedicationClientImpl::request(boost::beast::http::verb verb, std::string_view target,
                                                     const model::Kvnr& kvnr)
{
    auto req = Tee3Client::Request{verb, target, 11};
    req.set(Header::Tee3::XInsurantId, kvnr.id());
    req.set(Header::ContentType, static_cast<std::string>(MimeType::fhirJson));
    return req;
}

void EpaMedicationClientImpl::addLogData(const std::string& key, const std::any& data)
{
    mLogDataBag[key] = data;
}

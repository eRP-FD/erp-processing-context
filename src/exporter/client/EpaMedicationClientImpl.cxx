/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/client/EpaMedicationClientImpl.hxx"
#include "exporter/network/client/Tee3ClientPool.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/Uuid.hxx"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_future.hpp>
#include <rapidjson/document.h>
#include <future>

EpaMedicationClientImpl::EpaMedicationClientImpl(boost::asio::io_context& ioContext, std::string hostname,
                                                 std::shared_ptr<Tee3ClientPool> teeClientPool)

    : mIoContext{ioContext}
    , mHostname{std::move(hostname)}
    , mTeeClientPool{std::move(teeClientPool)}
{
}

EpaMedicationClientImpl::Response EpaMedicationClientImpl::sendProvidePrescription(const model::Kvnr& kvnr,
                                                                                   const std::string& payload)
{
    using boost::beast::http::verb;
    auto req = request(verb::post, "/epa/medication/api/v1/fhir/$provide-prescription-erp", kvnr);
    req.body() = payload;
    return sendRequestBlocking(std::move(req));
}


EpaMedicationClientImpl::Response EpaMedicationClientImpl::sendProvideDispensation(const model::Kvnr& kvnr,
                                                                                   const std::string& payload)
{
    using boost::beast::http::verb;
    auto req = request(verb::post, "/epa/medication/api/v1/fhir/$provide-dispensation-erp", kvnr);
    req.body() = payload;
    return sendRequestBlocking(std::move(req));
}

EpaMedicationClientImpl::Response EpaMedicationClientImpl::sendCancelPrescription(const model::Kvnr& kvnr,
                                                                                  const std::string& payload)
{
    using boost::beast::http::verb;
    auto req = request(verb::post, "/epa/medication/api/v1/fhir/$cancel-prescription-erp", kvnr);
    req.body() = payload;
    return sendRequestBlocking(std::move(req));
}

EpaMedicationClientImpl::Response EpaMedicationClientImpl::sendRequestBlocking(Tee3Client::Request req)
{
    std::future<Tee3Client::Response> fut = co_spawn(
        mIoContext,
        sendRequest(std::move(req)),
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
                co_return true;
            }
            catch (const std::exception& e)
            {
                co_return false;
            }
        },
        boost::asio::use_future);
    return fut.get();
}


boost::asio::awaitable<Tee3Client::Response> EpaMedicationClientImpl::sendRequest(Tee3Client::Request req)
{
    auto resp = co_await mTeeClientPool->sendTeeRequest(mHostname, std::move(req), mLogDataBag);
    Expect(resp.has_value(), "Response had an error");
    co_return resp.value();
}

Tee3Client::Request EpaMedicationClientImpl::request(boost::beast::http::verb verb, std::string_view target,
                                                     const model::Kvnr& kvnr)
{
    auto req = Tee3Client::Request{verb, target, 11};
    req.set(Header::Tee3::XInsurantId, kvnr.id());
    req.set(Header::XRequestId, Uuid().toString());
    req.set(Header::ContentType, static_cast<std::string>(MimeType::fhirJson));
    return req;
}

void EpaMedicationClientImpl::addLogData(const std::string& key, const std::any& data)
{
    mLogDataBag[key] = data;
}

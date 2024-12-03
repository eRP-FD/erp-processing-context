/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef MEDICATION_EXPORTER_EPAMEDICATIONCLIENTIMPL_HXX
#define MEDICATION_EXPORTER_EPAMEDICATIONCLIENTIMPL_HXX

#include "exporter/client/EpaMedicationClient.hxx"
#include "exporter/network/client/Tee3Client.hxx"
#include "shared/model/Kvnr.hxx"

#include <boost/asio/io_context.hpp>
#include <memory>
#include <string>

namespace model
{
class Kvnr;
}

class HsmPool;
class Tee3ClientPool;


/**
 * Implementation of the calls to an ePA backend. Note that
 * any of the methods must be called from outside the given io context,
 * otherwise they will lock.
 * As these calls are synchrounous and the Https Client is async, the io
 * context is being used to execute the https and TEE3 interaction and
 * then a blocking wait is done.
 */
class EpaMedicationClientImpl : public IEpaMedicationClient
{
public:
    EpaMedicationClientImpl(boost::asio::io_context& ioContext, std::string hostname,
                            std::shared_ptr<Tee3ClientPool> teeClientPool);

    Response sendProvidePrescription(const model::Kvnr& kvnr, const std::string& payload) override;
    Response sendProvideDispensation(const model::Kvnr& kvnr, const std::string& payload) override;
    Response sendCancelPrescription(const model::Kvnr& kvnr, const std::string& payload) override;
    void addLogData(const std::string& key, const std::any& data) override;

    bool testConnection();

private:
    Response sendRequestBlocking(Tee3Client::Request req);
    boost::asio::awaitable<Tee3Client::Response> sendRequest(Tee3Client::Request req);
    Tee3Client::Request request(boost::beast::http::verb verb, std::string_view target, const model::Kvnr& kvnr);

    boost::asio::io_context& mIoContext;
    std::string mHostname;
    std::shared_ptr<Tee3ClientPool> mTeeClientPool;
    std::unordered_map<std::string, std::any> mLogDataBag;
};

#endif//MEDICATION_EXPORTER_EPAMEDICATIONCLIENTIMPL_HXX

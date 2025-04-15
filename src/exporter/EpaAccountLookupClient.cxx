/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/EpaAccountLookupClient.hxx"
#include "BdeMessage.hxx"
#include "ExporterRequirements.hxx"
#include "exporter/network/client/HttpsClientPool.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "shared/beast/BoostBeastStringWriter.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/network/client/HttpsClient.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/TLog.hxx"

EpaAccountLookupClient::EpaAccountLookupClient(MedicationExporterServiceContext& serviceContext,
                                               std::string_view consentDecisionsEndpoint, std::string_view userAgent)
    : mServiceContext(serviceContext)
    , mConsentDecisionsEndpoint(consentDecisionsEndpoint)
    , mUserAgent(userAgent)
{
}

ClientResponse EpaAccountLookupClient::sendConsentDecisionsRequest(const std::string& xRequestId,
                                                                   const model::Kvnr& kvnr, const std::string& host,
                                                                   uint16_t port)
{
    A_25937.start("request to epa4all.de");
    Header header{HttpMethod::GET,
                  std::string{mConsentDecisionsEndpoint},
                  Header::Version_1_1,
                  {{Header::Tee3::XInsurantId, kvnr.id()},
                   {Header::Tee3::XUserAgent, mUserAgent},
                   {Header::Host, host},
                   {Header::XRequestId, xRequestId}},
                  HttpStatus::Unknown};
    ClientRequest request{header, {}};
    TVLOG(1) << "consent decision request for " << kvnr.id() << " to " << host << ":" << port;
    TVLOG(2) << "consent decision request: "
             << BoostBeastStringWriter::serializeRequest(request.getHeader(), request.getBody());

    auto client = mServiceContext.httpsClientPool(host)->acquire();

    BDEMessage bde;
    bde.mStartTime = model::Timestamp::now();
    bde.mInnerOperation = request.getHeader().target();
    bde.mHost = host;
    bde.mRequestId = xRequestId;
    BDEMessage::assignIfContains(mLookupClientLogContext, BDEMessage::prescriptionIdKey, bde.mPrescriptionId);
    BDEMessage::assignIfContains(mLookupClientLogContext, BDEMessage::lastModifiedTimestampKey, bde.mLastModified);
    BDEMessage::assignIfContains(mLookupClientLogContext, BDEMessage::hashedKvnrKey, bde.mHashedKvnr);

    try
    {
        auto result = sendWithRetry(*client, request);
        A_25937.finish();
        TVLOG(2) << "consent decision response: "
                 << BoostBeastStringWriter::serializeResponse(result.getHeader(), result.getBody());
        if (result.getHeader().status() != HttpStatus::OK)
        {
            std::ostringstream os{};
            os << "got response code " << result.getHeader().status() << " from account lookup to " << host << ":" << port;
            bde.mError = os.str();
        }
        const std::string code = result.getHeader().header(Header::InnerResponseCode).value_or("0");
        bde.mInnerResponseCode = static_cast<unsigned int>(std::strtoul(code.c_str(), nullptr, 10));
        bde.mResponseCode = static_cast<unsigned int>(toNumericalValue(result.getHeader().status()));
        bde.mEndTime = model::Timestamp::now();
        return result;
    } catch (const std::exception& e)
    {
        bde.mEndTime = model::Timestamp::now();
        bde.mError = e.what();
        Fail(e.what());
    }
}

IEpaAccountLookupClient& EpaAccountLookupClient::addLogAttribute(const std::string& key, const std::any& value)
{
    mLookupClientLogContext[key] = value;
    return *this;
}


ClientResponse EpaAccountLookupClient::sendWithRetry(HttpsClient& client, ClientRequest& request) const
{
    try
    {
        return client.send(request);
    }
    catch (const boost::beast::system_error& ex)
    {
        // as we are using the pool and the connection may have
        // been reset in the meantime, always retry once for closed connections
        auto ec = ex.code();
        if (boost::asio::ssl::error::stream_truncated == ec || boost::asio::error::connection_reset == ec ||
            boost::asio::error::not_connected == ec || boost::beast::http::error::end_of_stream == ec)
        {

            TLOG(INFO) << "Connection closed, reconnecting..";
            return client.send(request);
        }
        else
        {
            TLOG(WARNING) << "Unexpected error during send, will not retry: " << ec.message();
            throw;
        }
    }
}

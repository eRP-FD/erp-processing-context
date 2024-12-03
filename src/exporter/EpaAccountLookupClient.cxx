/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/EpaAccountLookupClient.hxx"
#include "BdeMessage.hxx"
#include "ExporterRequirements.hxx"
#include "shared/beast/BoostBeastStringWriter.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/network/client/HttpsClient.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/TLog.hxx"

EpaAccountLookupClient::EpaAccountLookupClient(uint16_t connectTimeoutSeconds, uint32_t resolveTimeoutMs,
                                               std::string_view consentDecisionsEndpoint, std::string_view userAgent,
                                               TlsCertificateVerifier tlsCertificateVerifier)
    : mConnectTimeoutSeconds(connectTimeoutSeconds)
    , mResolveTimeoutMs(resolveTimeoutMs)
    , mConsentDecisionsEndpoint(consentDecisionsEndpoint)
    , mUserAgent(userAgent)
    , mTlsCertificateVerifier(std::move(tlsCertificateVerifier))
{
}

ClientResponse EpaAccountLookupClient::sendConsentDecisionsRequest(const model::Kvnr& kvnr, const std::string& host,
                                                                   uint16_t port)
{
    A_25937.start("request to epa4all.de");
    HttpsClient client{
        ConnectionParameters{.hostname = host,
                             .port = std::to_string(port),
                             .connectionTimeoutSeconds = mConnectTimeoutSeconds,
                             .resolveTimeout = std::chrono::milliseconds{mResolveTimeoutMs},
                             .tlsParameters = TlsConnectionParameters{.certificateVerifier = mTlsCertificateVerifier}}};
    Header header{
        HttpMethod::GET,
        std::string{mConsentDecisionsEndpoint},
        Header::Version_1_1,
        {{Header::Tee3::XInsurantId, kvnr.id()}, {Header::Tee3::XUserAgent, mUserAgent}, {Header::Host, host}},
        HttpStatus::Unknown};
    ClientRequest request{header, {}};
    TVLOG(1) << "consent decision request for " << kvnr.id() << " to " << host << ":" << port;
    TVLOG(2) << "consent decision request: "
             << BoostBeastStringWriter::serializeRequest(request.getHeader(), request.getBody());

    BDEMessage bde;
    bde.mStartTime = model::Timestamp::now();
    bde.mInnerOperation = request.getHeader().target();
    bde.mHost = host;
    if (request.getHeader().hasHeader(Header::XRequestId))
    {
        bde.mRequestId = request.getHeader().header(Header::XRequestId).value();
    }

    auto result = client.send(request);
    A_25937.finish();
    TVLOG(2) << "consent decision response: "
             << BoostBeastStringWriter::serializeResponse(result.getHeader(), result.getBody());
    if (result.getHeader().status() != HttpStatus::OK)
    {
        std::ostringstream os{};
        os << "got response code " << result.getHeader().status() << " from account lookup to " << host
           << ":" << port;
        bde.mError = os.str();
        TLOG(WARNING) << bde.mError;
    }
    const std::string code = result.getHeader().header(Header::InnerResponseCode).value_or("0");
    bde.mInnerResponseCode = static_cast<unsigned int>(std::strtoul(code.c_str(), nullptr, 10));
    bde.mResponseCode = static_cast<unsigned int>(toNumericalValue(result.getHeader().status()));
    bde.mEndTime = model::Timestamp::now();
    return result;
}

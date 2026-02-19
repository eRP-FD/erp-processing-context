/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/client/BfArMClient.hxx"
#include "exporter/BdeMessage.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "exporter/client/TokenCache.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "exporter/model/trezept/ErpTPrescriptionCarbonCopy.hxx"
#include "shared/network/client/HttpsClient.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/UrlHelper.hxx"
#include "shared/util/Uuid.hxx"


using namespace medication::exporter::exceptions;


BfArMClient::BfArMClient(std::string clientId, std::shared_ptr<CrlProvider> crlProvider)
    : OAuthClientBase(std::move(clientId),
                      Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_BFARM_CLIENT_URL),
                      Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_BFARM_CLIENT_PORT),
                      std::move(crlProvider))
    , mClientSecret(Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_BFARM_CLIENT_SECRET))
{
    Configuration::instance().check(Configuration::ProcessType::MedicationExporter);
}


BfArMClient::~BfArMClient() = default;


void BfArMClient::sendCarbonCopy(const model::ErpTPrescriptionCarbonCopy& doc) const {
    const auto* const path = "/ords/rezepte/t-rezept/v1";
    const auto reqId = Uuid{}.toString();
    const auto accessTokenStr = getOrRefreshAccessToken();
    const auto response = handleCommonHttpErrors([this, &doc, &path, &reqId, &accessTokenStr](BDEMessage& message) {
        message.update(BDEMessage::Data{.innerOperation = path, .requestId = reqId});

        auto client = createClient(getHost(), getPort());

        A_27822.start("Use access token as string in Auth Bearer");
        A_27827.start("Transfer carbon copy to bfarm");
        auto request = ClientRequest{{HttpMethod::POST,
                                      path,
                                      Header::Version_1_1,
                                      {{Header::Authorization, "Bearer " + accessTokenStr},
                                       {Header::ContentType, "application/fhir+json"},
                                       {Header::Host, getHost() + ":" + getPort()},
                                       {Header::XRequestId, reqId}},
                                      HttpStatus::Unknown},
                                     doc.serializeToJsonString()};
        A_27827.finish();
        A_27822.finish();

        return client->send(request);
    });
}


ClientRequest BfArMClient::accessTokenRequest(const std::string& host, const std::string& port) const
{
    A_27821.start("Prepare token request");
    const auto* const path = "/ords/rezepte/oauth/token";
    const auto bearer = Base64::encode(getClientId() + ":" + mClientSecret);
    A_27821.finish();
    return {{HttpMethod::POST,
             path,
             Header::Version_1_1,
             {{Header::ContentType, MimeType::xWwwFormUrlEncoded},
              {Header::Host, host + ":" + port},
              {Header::Authorization, "Basic " + bearer},
              {Header::XRequestId, Uuid{}.toString()}},
             HttpStatus::Unknown},
            "grant_type=client_credentials"};
}

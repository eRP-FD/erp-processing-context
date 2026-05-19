/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/client/FhirVZDClient.hxx"
#include "exporter/BdeMessage.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "exporter/client/TokenCache.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/network/client/HttpsClient.hxx"
#include "shared/network/client/UrlRequestSender.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/UrlHelper.hxx"
#include "shared/util/Uuid.hxx"
#include <cstdlib>


using namespace medication::exporter::exceptions;


FhirVzdClient::FhirVzdClient(std::string clientId, std::shared_ptr<CrlProvider> crlProvider, const std::string& xContextId)
    : OAuthClientBase(
          std::move(clientId),
          Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_FHIR_VZD_CLIENT_TOKEN_URL),
          Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_FHIR_VZD_CLIENT_TOKEN_PORT),
          std::move(crlProvider), xContextId)
    , mClientSecret(
          Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_FHIR_VZD_CLIENT_SECRET))
    , mApiUrl(Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_FHIR_VZD_CLIENT_API_URL))
    , mApiPort(Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_FHIR_VZD_CLIENT_API_PORT))
    , mAuthTokenCache(std::make_unique<TokenCache>(TokenCache::TokenType::AuthToken))
{
    Configuration::instance().check(Configuration::ProcessType::MedicationExporter);
}


FhirVzdClient::~FhirVzdClient() = default;


model::Bundle FhirVzdClient::performSearch(const std::string& orgId) const
{
    ModelExpect(not orgId.empty(), "Missing orgId.");
    const auto reqId = Uuid{}.toString();
    const auto* const path = "/fdv/search/HealthcareService";
    const auto authToken = getOrRefreshAuthToken();
    auto response = handleCommonHttpErrors([this, &orgId, &reqId, &path, &authToken](BDEMessage& message) {
        message.update(BDEMessage::Data{.host = mApiUrl, .innerOperation = path, .requestId = reqId});
        auto client = createClient();

        A_27825.start("Actual fhir vzd search request");
        std::stringstream ss{};
        ss << path << "?organization.active=true&organization.identifier="
           << UrlHelper::escapeUrl(orgId) << "&_tag=ldap&_include=HealthcareService:organization"
           << "&_include=HealthcareService:location";

        A_27825.finish();
        client->setAdditionalHeaders({{Header::Authorization, "Bearer " + authToken}, {Header::XRequestId, reqId}});
        const auto url = UrlHelper::UrlParts(UrlHelper::HTTPS_PROTOCOL, mApiUrl,
                                             static_cast<int>(std::strtol(mApiPort.c_str(), nullptr, 10)), ss.str(), "")
                             .toString();
        return client->send(url, HttpMethod::GET, "");
    });

    const model::ResourceFactoryBase::Options opts{
        .validatorOptions{{.validateMetaProfiles = false,
                           .levels = {
                               .unknownMetaProfile = fhirtools::Severity::warning,
                           }}}};
    auto doc = model::NumberAsStringParserDocument::fromJson(response.getBody());
    return model::ResourceFactory<model::Bundle>::fromJson(std::move(doc), opts).getValidated(model::ProfileType::fhir);
}

bool FhirVzdClient::testConnection() const
{
    if (!OAuthClientBase::testConnection())
    {
        return false;
    }
    try
    {
        auto client = createClient();
        const auto url = UrlHelper::UrlParts(UrlHelper::HTTPS_PROTOCOL, mApiUrl,
                                             static_cast<int>(std::strtol(mApiPort.c_str(), nullptr, 10)), "", "")
                             .toString();
        auto response = client->send(url, HttpMethod::HEAD, "");
    }
    catch (const std::exception& e)
    {
        LOG(ERROR) << "Error testing connection to " << mApiUrl << ":" << mApiPort << ": " << e.what();
    }
    return false;
}


std::string FhirVzdClient::buildAccessTokenRequestBody() const
{
    std::stringstream ss;
    ss << "grant_type=client_credentials" << "&client_id=" << UrlHelper::escapeUrl(getClientId())
       << "&client_secret=" << UrlHelper::escapeUrl(mClientSecret);
    return ss.str();
}


std::string FhirVzdClient::getOrRefreshAuthToken() const
{
    {
        const std::lock_guard<std::mutex> lock(getTokenCacheMutex());
        if (mAuthTokenCache->isValid())
        {
            return mAuthTokenCache->getToken();
        }
    }
    const auto accessToken = getOrRefreshAccessToken();
    return fetchAuthToken(accessToken);
}


std::string FhirVzdClient::fetchAuthToken(const std::string& accessToken) const
{
    const auto* const path =  "/service-authenticate";
    return fetchToken(mApiUrl, mApiPort,
                      {{HttpMethod::GET,
                        path,
                        Header::Version_1_1,
                        {{Header::Authorization, "Bearer " + accessToken},
                         {Header::Host, mApiUrl + ":" + mApiPort}, {Header::XRequestId, Uuid{}.toString()}},
                        HttpStatus::Unknown},
                       ""},
                      *mAuthTokenCache);
}

ClientRequest FhirVzdClient::accessTokenRequest(const std::string& host, const std::string& port) const
{
    const auto* const path = "/auth/realms/Service-Authenticate/protocol/openid-connect/token";
    return {{HttpMethod::POST,
             path,
             Header::Version_1_1,
             {{Header::ContentType, MimeType::xWwwFormUrlEncoded}, {Header::Host, host + ":" + port}, {Header::XRequestId, Uuid{}.toString()}},
             HttpStatus::Unknown},
            buildAccessTokenRequestBody()};
}

void FhirVzdClient::invalidateTokenCache() const
{
    const std::lock_guard<std::mutex> lock(getTokenCacheMutex());
    getAccessTokenCache().invalidate();
    mAuthTokenCache->invalidate();
}

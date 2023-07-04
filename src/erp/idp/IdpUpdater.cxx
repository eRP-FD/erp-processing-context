/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/idp/IdpUpdater.hxx"

#include "erp/common/Constants.hxx"
#include "erp/crypto/Jwt.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/tsl/OcspHelper.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/JsonLog.hxx"
#include "erp/util/TLog.hxx"


namespace
{
    class ReportedException : public std::exception {};

    std::chrono::system_clock::duration getMaxCertificateAge (void)
    {
        const auto maxAgeInHours = Configuration::instance().getOptionalIntValue(ConfigurationKey::IDP_CERTIFICATE_MAX_AGE_HOURS, 24);
        return std::chrono::hours(maxAgeInHours);
    }


    Certificate readIdpCertificate(const std::string& derBase64String)
    {
        try
        {
            return Certificate::fromBase64Der(derBase64String);
        }
        catch(const std::runtime_error& e)
        {
            TslFail(std::string("can not read IDP certificate: ") + e.what(),
                    TslErrorCode::CERT_READ_ERROR);
        }
    }
}


/**
 * Make a GET request to the given `host` with the given URL `path` and return the returned body.
 */
std::string getBody (const UrlRequestSender& requestSender, const UrlHelper::UrlParts& url)
{
    ClientResponse response;
    boost::asio::io_context io;
    const auto port = std::to_string(url.mPort);
    auto resolverResults = boost::asio::ip::tcp::resolver{io}.resolve(url.mHost.c_str(), port.c_str());
    for (const auto& resolverEntry : resolverResults)
    {
        response = requestSender.send(resolverEntry.endpoint(), url, HttpMethod::GET, "");
        if (response.getHeader().status() == HttpStatus::OK)
        {
            break;
        }
        else
        {
            TLOG(WARNING) << "Connecting " << url.toString() << " (" << resolverEntry.endpoint().address() << ") failed";
        }
    }
    TVLOG(3) << "GET:" << url.toString() << " status=" << toString(response.getHeader().status()) << " body=" << response.getBody();
    Expect(response.getHeader().status() == HttpStatus::OK, "call failed");

    return response.getBody();
}


IdpUpdater::IdpUpdater (
    Idp& certificateHolder,
    TslManager& tslManager,
    const std::shared_ptr<UrlRequestSender>& urlRequestSender, // NOLINT(modernize-pass-by-value)
    std::shared_ptr<Timer> timerManager)
    : mUpdateFailureCount()
    , mCertificateHolder(certificateHolder)
    , mTslManager(tslManager)
    , mRequestSender(urlRequestSender)
    , mTimerJobToken(Timer::NotAJob)
    , mIsUpdateActive(false)
    , mUpdateHookId()
    , mCertificateMaxAge(getMaxCertificateAge())
    , mLastSuccessfulUpdate()
    , mTimerManager(std::move(timerManager))
    , updateIntervalMinutes(Configuration::instance().getIntValue(ConfigurationKey::IDP_UPDATE_INTERVAL_MINUTES))
    , noCertificateUpdateIntervalSeconds(
          Configuration::instance().getIntValue(ConfigurationKey::IDP_NO_VALID_CERTIFICATE_UPDATE_INTERVAL_SECONDS))
{
    Expect3(mTimerManager!=nullptr, "TimerManager missing", std::logic_error);
    SafeString idpSslRootCa;
    const std::string idpRootCaFile =
        Configuration::instance().getOptionalStringValue(ConfigurationKey::IDP_UPDATE_ENDPOINT_SSL_ROOT_CA_PATH, "");
    if ( ! idpRootCaFile.empty())
    {
        idpSslRootCa = FileHelper::readFileAsString(idpRootCaFile);
    }
    else
    {
        TVLOG(1) << "No IDP update endpoint SSL root CA configured";
    }
    if (mRequestSender == nullptr)
    {
        mRequestSender = std::make_shared<UrlRequestSender>(
            std::move(idpSslRootCa),
            static_cast<uint16_t>(Configuration::instance().getOptionalIntValue(
                ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS, Constants::httpTimeoutInSeconds)));
    }

    // Extract hostname and path from the update URL.
    const std::string updateUrl = Configuration::instance().getStringValue(ConfigurationKey::IDP_UPDATE_ENDPOINT);
    mUpdateUrl = std::make_unique<UrlHelper::UrlParts>(UrlHelper::parseUrl(updateUrl));
    ErpExpect(String::toLower(mUpdateUrl->mProtocol)=="https://", HttpStatus::InternalServerError, "IDP update URL must use https://");

    startUpdateTimer();

    // Also connect to the TslManager. If it updates, so will the IdpUpdater.
    mUpdateHookId = mTslManager.addPostUpdateHook([this] {
        update();
    });
}


IdpUpdater::~IdpUpdater (void)
{
    mTimerManager->cancel(mTimerJobToken);
    if (mUpdateHookId.has_value())
    {
        mTslManager.disablePostUpdateHook(*mUpdateHookId);
    }
}


void IdpUpdater::update (void)
{
    // An update of the IdpUpdater can trigger an update of the TslManager. That in turn would call this method again.
    // Prevent this second call.
    const bool wasAlreadyActive = mIsUpdateActive.exchange(true);
    if (wasAlreadyActive)
        return;

    try
    {
        try
        {
            auto certificate = getUpToDateCertificate();
            mCertificateHolder.setCertificate(std::move(certificate));
            reportUpdateStatus(UpdateStatus::Success, "");
            mIsUpdateActive = false;
        }
        catch(...)
        {
            mIsUpdateActive = false;
            throw;
        }
    }
    catch(const ReportedException&)
    {
        // Error has already been reported. There is nothing more to do.
    }
    catch(...)
    {
        // An exception that has not been caught earlier.
        reportUpdateStatus(UpdateStatus::UnknownFailure, "");
    }
}


Certificate IdpUpdater::getUpToDateCertificate (void)
{
    // Many details of the implementation of the individual steps are based on https://dth01.ibmgcloud.net/jira/browse/ERP-4850
    // and https://dth01.ibmgcloud.net/confluence/pages/viewpage.action?spaceKey=ERP&title=Discovery+Document

    const auto uri = downloadAndParseWellknown();
    const auto certificateChain = downloadAndParseDiscovery(uri);
    verifyCertificate(certificateChain);
    return certificateChain.front();
}


UrlHelper::UrlParts IdpUpdater::downloadAndParseWellknown (void)
{
    try
    {
        return doParseWellknown(doDownloadWellknown());
    }
    catch(const std::exception& e)
    {
        reportUpdateStatus(UpdateStatus::WellknownDownloadFailed, e.what());
        throw ReportedException();
    }
}


std::string IdpUpdater::doDownloadWellknown (void)
{
    return getBody(*mRequestSender, *mUpdateUrl);//"idp.lu.erezepttest.net", "/.well-known/openid-configuration");
}


UrlHelper::UrlParts IdpUpdater::doParseWellknown (std::string&& wellKnownConfiguration)
{
    const auto jwt = JWT(wellKnownConfiguration);
    // According to https://dth01.ibmgcloud.net/jira/browse/ERP-4850?focusedCommentId=128946&page=com.atlassian.jira.plugin.system.issuetabpanels:comment-tabpanel#comment-128946
    // a verification of the discovery document (our `jwt`) is not necessary.

    // Get the uri_puk_idp_sig claim and extract hostname and path.
    const auto uri = jwt.stringForClaim("uri_puk_idp_sig");
    Expect(uri.has_value(), "signature URI missing");
    return UrlHelper::parseUrl(uri.value());
}


std::vector<Certificate> IdpUpdater::downloadAndParseDiscovery (const UrlHelper::UrlParts& url)
{
    try
    {
        return doParseDiscovery(doDownloadDiscovery(url));
    }
    catch(const TslError& e)
    {
        // TslError has own json representation and must be output to error-log
        TLOG(ERROR) << e.what();

        reportUpdateStatus(UpdateStatus::DiscoveryDownloadFailed, "TslError");
        throw ReportedException();
    }
    catch(const std::exception& e)
    {
        reportUpdateStatus(UpdateStatus::DiscoveryDownloadFailed, e.what());
        throw ReportedException();
    }
}


std::string IdpUpdater::doDownloadDiscovery (const UrlHelper::UrlParts& url)
{
    // Download the JWK set.
    return getBody(*mRequestSender, url);
}


std::vector<Certificate> IdpUpdater::doParseDiscovery (std::string&& jwkString)
{
    // Interpretation of the returned JWK according to https://dth01.ibmgcloud.net/jira/browse/ERP-4850?focusedCommentId=128571&page=com.atlassian.jira.plugin.system.issuetabpanels:comment-tabpanel#comment-128571
    // and https://tools.ietf.org/html/rfc7517.
    rapidjson::Document jwk;
    jwk.Parse(jwkString);
    Expect( ! jwk.HasParseError(), "parsing of IDP JWK failed");

    const auto* x5c = rapidjson::Pointer("/x5c").Get(jwk);
    Expect(x5c != nullptr, "IDP JWK does not have an /x5c entry");
    Expect(x5c->IsArray(), "IDP JWK's x5c value is not an array (of certificates)");
    const auto x5cArray = x5c->GetArray();
    std::vector<Certificate> certificates;
    certificates.reserve(x5cArray.Size());
    for (const auto& value : x5cArray)
    {
        Expect(value.IsString(), "certificate value is not a string");
        certificates.emplace_back(readIdpCertificate(value.GetString()));
        TVLOG(1) << "successfully got IDP certificate";
    }

    return certificates;
}


void IdpUpdater::verifyCertificate (const std::vector<Certificate>& certificateChain)
{
    try
    {
        doVerifyCertificate(certificateChain);
    }
    catch(const std::exception& e)
    {
        reportUpdateStatus(UpdateStatus::VerificationFailed, e.what());
        throw ReportedException();
    }
}


// GEMREQ-start A_20158
void IdpUpdater::doVerifyCertificate (const std::vector<Certificate>& certificateChain)
{
    A_20158_01.start("verify updated IDP signer certificate");

    Expect( ! certificateChain.empty(), "no certificate found in discovery document");
    Expect(certificateChain.size()==1, "got more than one IDP signer certificate in the discovery document");

    auto x509 = X509Certificate::createFromX509Pointer(certificateChain.front().toX509().removeConst().get());
    mTslManager.verifyCertificate(
        TslMode::TSL,
        x509,
        {CertificateType::C_FD_SIG},
        {OcspCheckDescriptor::OcspCheckMode::PROVIDED_OR_CACHE,
         {std::nullopt, OcspHelper::getOcspGracePeriod(TslMode::TSL)},
         {}});

    Expect(x509.checkValidityPeriod(), "Invalid IDP certificate");

    A_20158_01.finish();
}
// GEMREQ-end A_20158


void IdpUpdater::reportUpdateStatus (const UpdateStatus status, std::string_view details)
{
    if (status == UpdateStatus::Success)
    {
        JsonLog(LogId::IDP_UPDATE_SUCCESS, JsonLog::makeVLogReceiver(0))
            .message(getMessageText(status))
            .details(details)
            .keyValue("failedRetries", std::to_string(mUpdateFailureCount));
        mUpdateFailureCount = 0;

        mLastSuccessfulUpdate = std::chrono::system_clock::now();
    }
    else
    {
        ++mUpdateFailureCount;
        JsonLog(LogId::IDP_UPDATE_FAILED, JsonLog::makeWarningLogReceiver(), true)
            .message(getMessageText(status))
            .details(details)
            .keyValue("failedRetries", std::to_string(mUpdateFailureCount));

        if (std::chrono::system_clock::now() - mLastSuccessfulUpdate >= mCertificateMaxAge)
        {
            JsonLog(LogId::IDP_UPDATE_FAILED, JsonLog::makeErrorLogReceiver(), false)
                .message("disabled IDP signer certificate because last successful update is older than " +
                         std::to_string(std::chrono::duration_cast<std::chrono::hours>(mCertificateMaxAge).count()) +
                         " hours (or no update at all)");
            mCertificateHolder.resetCertificate();
        }
    }
}


void IdpUpdater::setCertificateMaxAge (std::chrono::system_clock::duration maxAge)
{
    mCertificateMaxAge = maxAge;
}


void IdpUpdater::startUpdateTimer()
{
    const bool healthy = mCertificateHolder.isHealthy();
    mTimerManager->cancel(mTimerJobToken);
    A_20974.start("update and verify the IDP signer certificate every hour");
    mTimerJobToken = mTimerManager->runIn(healthy ? std::chrono::minutes(updateIntervalMinutes)
                                                  : std::chrono::seconds(noCertificateUpdateIntervalSeconds),
                                          [this] {
                                              update();
                                              startUpdateTimer();
                                          });
    A_20974.finish();

}


std::string IdpUpdater::getMessageText (const UpdateStatus status) const
{
    switch(status)
    {
        case UpdateStatus::Success:
            return "IDP signer certificate successfully updated and verified in attempt";
        case UpdateStatus::WellknownDownloadFailed:
            return "download or parsing of IDP openid configuration failed";
        case UpdateStatus::DiscoveryDownloadFailed:
            return "download or parsing of IDP discovery document failed";
        case UpdateStatus::VerificationFailed:
            return "verification of IDP signer certificate failed";
        default:
            return "Unknown";
    }
}

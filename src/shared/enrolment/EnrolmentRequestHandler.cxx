/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/enrolment/EnrolmentRequestHandler.hxx"
#include "shared/server/BaseSessionContext.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/enrolment/VsdmHmacKey.hxx"
#include "shared/erp-serverinfo.hxx"
#include "shared/hsm/ErpTypes.hxx"
#include "shared/hsm/VsdmKeyBlobDatabase.hxx"
#include "shared/server/BaseServiceContext.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/tpm/PcrSet.hxx"
#include "shared/tpm/Tpm.hxx"
#include "shared/tsl/OcspHelper.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Hash.hxx"
#include "shared/util/TLog.hxx"

#if WITH_HSM_TPM_PRODUCTION > 0
#include "shared/enrolment/EnrolmentHelper.hxx"
#endif

using namespace ::std::literals;

void EnrolmentRequestHandlerBasicAuthentication::handleBasicAuthentication(
    const BaseSessionContext& session, ConfigurationKey credentialsKey) const
{
    SafeString secret(Configuration::instance().getSafeStringValue(credentialsKey));
    ErpExpect(session.request.header().hasHeader(Header::Authorization), HttpStatus::Unauthorized,
              "Authorization required.");
    auto parts = String::split(session.request.header().header(Header::Authorization).value_or(""), " ");
    if (parts.size() == 2)
    {
        SafeString authSecret(std::move(parts[1]));
        ErpExpect(String::toLower(parts[0]) == "basic" && authSecret == secret, HttpStatus::Unauthorized,
                  "Unauthorized");
    }
    else
    {
        ErpExpect(false, HttpStatus::Unauthorized, "Unauthorized (unsupported authorization content.)");
    }
}

namespace
{

    /**
     * Acquire a TPM instance on demand so that the TPM is not blocked longer than necessary.
     */
    std::unique_ptr<Tpm> getTpm (EnrolmentSession& session)
    {
        auto tpm = session.baseServiceContext.getTpmFactory()(*session.baseServiceContext.getBlobCache());
        Expect(tpm!=nullptr, "TPM factory did not produce TPM instance");
        return tpm;
    }


    /**
     * Base64 decode keyBase64, then apply HMAC_SHA256, then base64 encode.
     */
    std::string base64Hmac (const std::string& keyBase64, const std::string& message)
    {
        const auto key = Base64::decode(keyBase64);
        const auto hash = Hash::hmacSha256(key, message);
        return Base64::encode(hash);
    }


    void verifyPcrSet (const std::vector<size_t>& inputPcrSet)
    {
        // Compare the input pcr set against the one in the configuration.
        // A difference is regarded as error. This is the first step in removing the pcr set as input parameter.
        try
        {
            const auto expectedPcrSet =
                PcrSet::fromString(Configuration::instance().getStringValue(ConfigurationKey::PCR_SET));
            const auto givenPcrSet = PcrSet::fromList(inputPcrSet);
            ErpExpect(givenPcrSet == expectedPcrSet, HttpStatus::BadRequest,
                "given pcr set " + givenPcrSet.toString() + " is not the same as the expected " + expectedPcrSet.toString());
        }
        catch (const std::runtime_error& e)
        {
            TLOG(WARNING) << "caught exception '" << e.what() << "' while verifying pcr set, converting to BadRequest";
            ErpFail(HttpStatus::BadRequest, e.what());
        }
    }

    ::std::string getBase64PcrHash(::std::string_view base64Quote, ::BaseServiceContext& serviceContext)
    {
        auto hsmPoolSession = serviceContext.getHsmPool().acquire();
        const auto quoteData = ::ErpVector{::Base64::decode(base64Quote)};
        const auto pcrHash = hsmPoolSession.session().parseQuote(quoteData).pcrHash;
        return ::Base64::encode(pcrHash);
    }

    bool isCertificateValid(const std::string& certificate, TslManager& tslManager, CertificateType certificateType)
    {
        try
        {
            // TODO: get rid of the back and forth once X509Certificate is merged with Certificate
            const auto base64DerCertificate = Certificate::fromPem(certificate).toBase64Der();
            auto x509Certificate = X509Certificate::createFromBase64(base64DerCertificate);
            tslManager.verifyCertificate(
                TslMode::TSL,
                x509Certificate,
                {certificateType},
                {OcspCheckDescriptor::OcspCheckMode::PROVIDED_OR_CACHE,
                 {std::nullopt, OcspHelper::getOcspGracePeriod(TslMode::TSL)},
                 {}});

            return true;
        }
        catch (const std::exception& exception)
        {
            TVLOG(1) << "Certificate validation failed: " << exception.what();
            return false;
        }
    }

enum ResourceType
{
    Task,
    Communication,
    AuditLog,
    ChargeItem
};

BlobType getBlobTypeForResourceType (const ResourceType resourceType)
{
    switch(resourceType)
    {
        case Task: return BlobType::TaskKeyDerivation;
        case Communication: return BlobType::CommunicationKeyDerivation;
        case AuditLog: return BlobType::AuditLogKeyDerivation;
        case ChargeItem: return ::BlobType::ChargeItemKeyDerivation;
    }
    Fail("unsupported resource type");
}
}

Operation EnrolmentRequestHandlerBase::getOperation (void) const
{
    return Operation::UNKNOWN;
}


void EnrolmentRequestHandlerBase::handleRequest (EnrolmentSession& session)
{
    try
    {
        handleBasicAuthentication(session, ConfigurationKey::ENROLMENT_API_CREDENTIALS);

        const auto document = doHandleRequest(session);
        session.response.setBody(document.serializeToString());
        if (session.response.getHeader().status() == HttpStatus::Unknown)
            session.response.setStatus(HttpStatus::OK);
        // else don't overwrite if status has already been set explicitly.
        session.response.setHeader(::Header::ContentType,::ContentMimeType::jsonUtf8);
    }
    catch (const ErpException& e)
    {
        session.accessLog.locationFromException(e);
        session.accessLog.error("caught ErpException in EnrolmentRequestHandlerBase::handleRequest");
        TVLOG(1) << "exception details: " << e.what();
        session.response.setStatus(e.status());
        session.response.withoutBody();
    }
    catch (const std::exception& e)
    {
        session.accessLog.locationFromException(e);
        session.accessLog.error("caught std::exception in EnrolmentRequestHandlerBase::handleRequest");
        TVLOG(1) << "exception details: " << e.what();
        session.response.setStatus(HttpStatus::InternalServerError);
        session.response.withoutBody();
    }
    catch (...)
    {
        session.accessLog.error("caught exception in EnrolmentRequestHandlerBase::handleRequest");
        session.response.setStatus(HttpStatus::InternalServerError);
        session.response.withoutBody();
    }
}


PutBlobHandler::PutBlobHandler (const BlobType blobType, std::string&& endpointDisplayname)
    : mBlobType(blobType),
      mEndpointDisplayname(endpointDisplayname)
{
}


EnrolmentModel PutBlobHandler::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("PUT " + mEndpointDisplayname);

    EnrolmentModel requestData (session.request.getBody());
    auto blobId = requestData.getDecodedErpVector(requestId);
    auto blobData = requestData.getDecodedString(requestBlobData);
    const auto blobGeneration = requestData.getInt64(requestBlobGeneration);

    BlobDatabase::Entry entry;
    entry.type = mBlobType;
    entry.name = blobId;
    entry.blob = ErpBlob(std::move(blobData), gsl::narrow_cast<ErpBlob::GenerationId>(blobGeneration));
    session.baseServiceContext.getBlobCache()->storeBlob(std::move(entry));

    session.response.setStatus(HttpStatus::Created);
    return {};
}


DeleteBlobHandler::DeleteBlobHandler (const BlobType blobType, std::string&& endpointDisplayname)
    : mBlobType(blobType),
      mEndpointDisplayname(endpointDisplayname)
{
}


EnrolmentModel DeleteBlobHandler::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("DELETE " + mEndpointDisplayname);

    EnrolmentModel requestData (session.request.getBody());
    auto blobId = requestData.getDecodedErpVector(requestId);

    session.baseServiceContext.getBlobCache()->deleteBlob(mBlobType, blobId);

    session.response.setStatus(HttpStatus::NoContent);
    return {};
}




namespace enrolment
{

EnrolmentModel GetEnclaveStatus::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("GET /Enrolment/EnclaveStatus");
    ErpExpect(session.request.getBody().empty(), HttpStatus::BadRequest, "no request body expected");

    const auto enrolmentStatus = EnrolmentData::getEnclaveStatus(*session.baseServiceContext.getBlobCache());

    EnrolmentModel responseData;
    responseData.set(responseEnclaveId, session.baseServiceContext.getEnrolmentData().enclaveId.toString());
    responseData.set(responseEnclaveTime, std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    responseData.set(responseEnrolmentStatus, magic_enum::enum_name(enrolmentStatus));
    const auto pcrSet = ::PcrSet::fromString(::Configuration::instance().getStringValue(::ConfigurationKey::PCR_SET));
    responseData.set(responsePcrSet, pcrSet.toString());

    auto hsmPoolSession = session.baseServiceContext.getHsmPool().acquire();
    const auto nonce = ::Base64::encode(hsmPoolSession.session().getNonce(0).nonce);
    const auto quote =
        getTpm(session)->getQuote(::Tpm::QuoteInput{nonce, pcrSet.toPcrList(), ::std::string{hashAlgorithm}}, true);
    responseData.set(responsePcrHash, getBase64PcrHash(quote.quotedDataBase64, session.baseServiceContext));

    responseData.set(responseVersionRelease, ErpServerInfo::ReleaseVersion());
    responseData.set(responseVersionReleasedate, ErpServerInfo::ReleaseDate());
    responseData.set(responseVersionBuild, ErpServerInfo::BuildVersion());
    responseData.set(responseVersionBuildType, ErpServerInfo::BuildType());
    return responseData;
}


EnrolmentModel GetEndorsementKey::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("GET /Enrolment/EndorsementKey");
    ErpExpect(session.request.getBody().empty(), HttpStatus::BadRequest, "no body expected");

    const auto endorsementKey = getTpm(session)->getEndorsementKey();
    EnrolmentModel responseData;
    responseData.set(responseCertificate, endorsementKey.certificateBase64);
    responseData.set(responseEkName, endorsementKey.ekNameBase64);
    return responseData;
}


EnrolmentModel GetAttestationKey::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("GET /Enrolment/AttestationKey");

    const auto attestationKey = getTpm(session)->getAttestationKey(true);

    EnrolmentModel responseData;
    responseData.set(responsePublicKey, attestationKey.publicKeyBase64);
    responseData.set(responseAkName, attestationKey.akNameBase64);
    return responseData;
}


EnrolmentModel PostAuthenticationCredential::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("POST /Enrolment/AuthenticateCredential");

    Tpm::AuthenticateCredentialInput input;
    EnrolmentModel requestData (session.request.getBody());
    input.secretBase64 = requestData.getString(requestSecret);
    input.credentialBase64 = requestData.getString(requestCredential);
    input.akNameBase64 = requestData.getString(requestAkName);

    const auto output = getTpm(session)->authenticateCredential(std::move(input), true);

    EnrolmentModel responseData;
    responseData.set(responsePlainTextCredential, output.plainTextCredentialBase64);
    return responseData;
}


EnrolmentModel PostGetQuote::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("POST /Enrolment/GetQuote");

    Tpm::QuoteInput input;
    EnrolmentModel requestData (session.request.getBody());
    input.nonceBase64 = base64Hmac(requestData.getString(requestNonce), "ERP_ENROLLMENT");
    input.pcrSet = requestData.getSizeTVector(requestPcrSet);
    input.hashAlgorithm = requestData.getString(requestHashAlgorithm);

    verifyPcrSet(input.pcrSet);

    const auto output = getTpm(session)->getQuote(std::move(input), true);

    EnrolmentModel responseData;
    responseData.set(responseQuotedData, output.quotedDataBase64);
    responseData.set(responseQuoteSignature, output.quoteSignatureBase64);
    responseData.set(responsePcrSet, ::PcrSet::fromList(requestData.getSizeTVector(requestPcrSet)).toString());
    responseData.set(responsePcrHash, getBase64PcrHash(output.quotedDataBase64, session.baseServiceContext));

    return responseData;
}


PutKnownEndorsementKey::PutKnownEndorsementKey (void)
    : PutBlobHandler(BlobType::EndorsementKey, "/Enrolment/KnownEndorsementKey")
{
}


DeleteKnownEndorsementKey::DeleteKnownEndorsementKey (void)
    : DeleteBlobHandler(BlobType::EndorsementKey, "/Enrolment/KnownEndorsementKey")
{
}


EnrolmentModel PutKnownAttestationKey::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("PUT /Enrolment/KnownAttestationKey");

    EnrolmentModel requestData (session.request.getBody());
    auto blobId = requestData.getDecodedErpVector(requestId);
    // This value is only optional during a transition phase. After that it will become mandatory.
    auto optionalAkName = requestData.getOptionalDecodedString(requestAkName);
    auto blobData = requestData.getDecodedString(requestBlobData);
    const auto blobGeneration = requestData.getInt64(requestBlobGeneration);

    BlobDatabase::Entry entry;
    entry.type = BlobType::AttestationPublicKey;
    entry.name = blobId;
    entry.blob = ErpBlob(std::move(blobData), gsl::narrow_cast<ErpBlob::GenerationId>(blobGeneration));

    if (optionalAkName.has_value())
    {
        // Use blobId as 32 byte hash id/name and akName as, well, akName.
        ErpExpect(blobId.size() == 32, HttpStatus::BadRequest, "blob name has a wrong length");
        ErpExpect(optionalAkName->size() == TpmObjectNameLength, HttpStatus::BadRequest, "ak name has a wrong length");
        entry.metaAkName = ErpArray<TpmObjectNameLength>::create(optionalAkName.value());
        std::copy(optionalAkName->begin(), optionalAkName->end(), entry.metaAkName->begin());
    }
    else
    {
        // Interpret blobId as akName.
        ErpExpect(blobId.size() == TpmObjectNameLength, HttpStatus::BadRequest, "name has a wrong length and can not be used as ak name");
        entry.metaAkName = blobId.toArray<TpmObjectNameLength>();
    }

    session.baseServiceContext.getBlobCache()->storeBlob(std::move(entry));

    session.response.setStatus(HttpStatus::Created);
    return {};
}


DeleteKnownAttestationKey::DeleteKnownAttestationKey (void)
    : DeleteBlobHandler(BlobType::AttestationPublicKey, "/Enrolment/KnownAttestationKey")
{
}


EnrolmentModel PutKnownQuote::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("PUT /Enrolment/KnownQuote");

    EnrolmentModel requestData(session.request.getBody());

    auto pcrSet = requestData.getOptionalSizeTVector(requestPcrSet);
    if (pcrSet)
    {
        verifyPcrSet(pcrSet.value());
    }
    else
    {
        pcrSet =
            ::PcrSet::fromString(::Configuration::instance().getStringValue(::ConfigurationKey::PCR_SET)).toPcrList();
    }

    ::BlobDatabase::Entry entry;
    entry.type = ::BlobType::Quote;
    entry.name = requestData.getDecodedErpVector(requestId);
    const auto blobGeneration =
        ::gsl::narrow_cast<::ErpBlob::GenerationId>(requestData.getInt64(requestBlobGeneration));
    entry.blob = ::ErpBlob{requestData.getDecodedString(requestBlobData), blobGeneration};
    entry.pcrSet = ::PcrSet::fromList(pcrSet.value());

    auto hsmPoolSession = session.baseServiceContext.getHsmPool().acquire();
    const auto nonce = ::Base64::encode(hsmPoolSession.session().getNonce(0).nonce);
    const auto quote =
        getTpm(session)->getQuote(::Tpm::QuoteInput{nonce, pcrSet.value(), ::std::string{hashAlgorithm}}, true);
    entry.pcrHash = ::ErpVector{::Base64::decode(getBase64PcrHash(quote.quotedDataBase64, session.baseServiceContext))};
    auto& blobCache = *session.baseServiceContext.getBlobCache();

#if WITH_HSM_TPM_PRODUCTION > 0
    try
    {
        session.baseServiceContext.getHsmPool().setTeeToken(EnrolmentHelper::getTeeToken(hsmPoolSession.session(), blobCache, entry.blob));
    }
    catch (...)
    {
        ErpFail(::HttpStatus::BadRequest, "Unable to create TEE Token for Quote.");
    }
#endif

    blobCache.storeBlob(std::move(entry));

    session.response.setStatus(HttpStatus::Created);

    return {};
}


DeleteKnownQuote::DeleteKnownQuote (void)
    : DeleteBlobHandler(BlobType::Quote, "/Enrolment/KnownQuote")
{
}


EnrolmentModel PutEciesKeypair::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("PUT /Enrolment/EciesKeypair");

    EnrolmentModel requestData (session.request.getBody());
    auto blobId = requestData.getDecodedErpVector(requestId);
    auto blobData = requestData.getDecodedString(requestBlobData);
    const auto blobGeneration = requestData.getInt64(requestBlobGeneration);
    const auto expiryDateTime = requestData.getInt64(requestExpiryDateTime);

    BlobDatabase::Entry entry;
    entry.type = BlobType::EciesKeypair;
    entry.name = blobId;
    entry.blob = ErpBlob(std::move(blobData), gsl::narrow_cast<ErpBlob::GenerationId>(blobGeneration));
    entry.expiryDateTime = std::chrono::system_clock::time_point(std::chrono::seconds(expiryDateTime));
    session.baseServiceContext.getBlobCache()->storeBlob(std::move(entry));

    session.response.setStatus(HttpStatus::Created);
    return {};
}


EnrolmentModel DeleteEciesKeypair::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("DELETE /Enrolment/EciesKeypair");

    EnrolmentModel requestData (session.request.getBody());
    auto blobId = requestData.getDecodedErpVector(requestId);

    session.baseServiceContext.getBlobCache()->deleteBlob(BlobType::EciesKeypair, blobId);

    session.response.setStatus(HttpStatus::NoContent);
    return {};
}


EnrolmentModel PutDerivationKey::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("PUT /Enrolment/<Type>/DerivationKey");

    const auto resourceTypeName = session.request.getPathParameter("Type");
    ErpExpect(resourceTypeName.has_value(), HttpStatus::BadRequest, "no resource type given");
    const auto resourceType = magic_enum::enum_cast<ResourceType>(resourceTypeName.value());
    ErpExpect(resourceType.has_value(), HttpStatus::BadRequest, "invalid resource type given");

    EnrolmentModel requestData (session.request.getBody());
    auto blobId = requestData.getDecodedErpVector(requestId);
    auto blobData = requestData.getDecodedString(requestBlobData);
    const auto blobGeneration = requestData.getInt64(requestBlobGeneration);

    BlobDatabase::Entry entry;
    entry.type = getBlobTypeForResourceType(resourceType.value());
    entry.name = blobId;
    entry.blob = ErpBlob(std::move(blobData), gsl::narrow_cast<ErpBlob::GenerationId>(blobGeneration));
    session.baseServiceContext.getBlobCache()->storeBlob(std::move(entry));

    session.response.setStatus(HttpStatus::Created);
    return {};
}


EnrolmentModel DeleteDerivationKey::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("DELETE /Enrolment/<Type>/DerivationKey");

    const auto resourceTypeName = session.request.getPathParameter("Type");
    ErpExpect(resourceTypeName.has_value(), HttpStatus::BadRequest, "no resource type given");
    const auto resourceType = magic_enum::enum_cast<ResourceType>(resourceTypeName.value());
    ErpExpect(resourceType.has_value(), HttpStatus::BadRequest, "invalid resource type given");

    EnrolmentModel requestData (session.request.getBody());
    const auto blobType = getBlobTypeForResourceType(resourceType.value());
    const auto blobId = requestData.getDecodedErpVector(requestId);

    session.baseServiceContext.getBlobCache()->deleteBlob(blobType, blobId);

    session.response.setStatus(HttpStatus::NoContent);
    return {};
}


PutKvnrHashKey::PutKvnrHashKey (void)
    : PutBlobHandler(BlobType::KvnrHashKey, "/Enrolment/KvnrHashKey")
{
}


PutTelematikIdHashKey::PutTelematikIdHashKey (void)
    : PutBlobHandler(BlobType::TelematikIdHashKey, "/Enrolment/TelematikIdHashKey")
{
}


DeleteKvnrHashKey::DeleteKvnrHashKey (void)
    : DeleteBlobHandler(BlobType::KvnrHashKey, "/Enrolment/KvnrHashKey")
{
}


DeleteTelematikIdHashKey::DeleteTelematikIdHashKey (void)
    : DeleteBlobHandler(BlobType::TelematikIdHashKey, "/Enrolment/TelematikIdHashKey")
{
}


PostVauSig::PostVauSig()
    : PostBlobHandler(BlobType::VauSig, "/Enrolment/VauSig")
{
}


EnrolmentModel PostVauSig::doHandleRequest(EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("POST /Enrolment/VauSig");

    try
    {
        ::EnrolmentModel requestData{session.request.getBody()};

        ::BlobDatabase::Entry entry;
        entry.type = ::BlobType::VauSig;
        entry.name = requestData.getDecodedErpVector(requestId);
        entry.blob =
            ::ErpBlob(requestData.getDecodedString(requestBlobData),
                      ::gsl::narrow_cast<::ErpBlob::GenerationId>(requestData.getInt64(requestBlobGeneration)));

        // the request data contains a PEM certificate, but encoded in base64 again
        entry.certificate = requestData.getDecodedString(requestCertificate);

        auto& tslManager = session.baseServiceContext.getTslManager();
        if (! isCertificateValid(*entry.certificate, tslManager, CertificateType::C_FD_OSIG))
        {
            session.response.setStatus(HttpStatus::BadRequest);
            EnrolmentModel response{};
            response.set("/code", "CERTIFICATE_INVALID");
            response.set("/description", "The certificate's details could not be verified for an OSIG certificate.");
            return response;
        }

        try
        {
            const auto validFrom = requestData.getOptionalInt64(requestValidFrom);
            // Allow a grace period when checking for start dates in the past.
            if (validFrom.has_value())
            {
                ErpExpect(::std::chrono::system_clock::time_point{::std::chrono::seconds{*validFrom}} >=
                              (::std::chrono::system_clock::now() - 5min),
                          ::HttpStatus::BadRequest, "timestamp lies in the past");

                entry.startDateTime = ::std::chrono::system_clock::time_point{::std::chrono::seconds{*validFrom}};
            }
            else
            {
                entry.startDateTime = ::std::chrono::system_clock::now();
            }
        }
        catch (const ErpException& exception)
        {
            session.accessLog.error("caught ErpException in PutVauSig::doHandleRequest");
            TVLOG(1) << "exception details: " << exception.what();

            session.response.setStatus(::HttpStatus::BadRequest);
            ::EnrolmentModel response;
            response.set("/code", "START_NOT_VALID");
            response.set("/description", "The value for valid-from is invalid or in the past.");
            return response;
        }

        session.baseServiceContext.getBlobCache()->storeBlob(std::move(entry));

        session.response.setStatus(HttpStatus::Created);

        return {};
    }
    catch (const ErpException& exception)
    {
        if (exception.status() == ::HttpStatus::BadRequest)
        {
            session.accessLog.error("caught ErpException in PutVauSig::doHandleRequest");
            TVLOG(1) << "exception details: " << exception.what();

            session.response.setStatus(exception.status());
            ::EnrolmentModel response;
            response.set("/code", "INVALID");
            response.set("/description", "The input could not be parsed or input validation failed.");
            return response;
        }
        else
        {
            throw;
        }
    }
}

DeleteVauSig::DeleteVauSig()
    : DeleteBlobHandler(BlobType::VauSig, "/Enrolment/VauSig")
{
}

PostVauAut::PostVauAut()
    : PostBlobHandler(BlobType::VauAut, "/Enrolment/VauAut")
{
}

EnrolmentModel PostVauAut::doHandleRequest(EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("POST /Enrolment/VauAut");

    try
    {
        EnrolmentModel requestData{session.request.getBody()};

        BlobDatabase::Entry entry;
        entry.type = BlobType::VauAut;
        entry.name = requestData.getDecodedErpVector(requestId);
        entry.blob =
            ErpBlob(requestData.getDecodedString(requestBlobData),
                      gsl::narrow_cast<ErpBlob::GenerationId>(requestData.getInt64(requestBlobGeneration)));

        // the request data contains a PEM certificate, but encoded in base64 again
        entry.certificate = requestData.getDecodedString(requestCertificate);

        auto& tslManager = session.baseServiceContext.getTslManager();
        if (! isCertificateValid(*entry.certificate, tslManager, CertificateType::C_FD_AUT))
        {
            session.response.setStatus(HttpStatus::BadRequest);
            EnrolmentModel response{};
            response.set("/code", "CERTIFICATE_INVALID");
            response.set("/description", "The certificate's details could not be verified for an AUT certificate.");
            return response;
        }

        try
        {
            const auto validFrom = requestData.getOptionalInt64(requestValidFrom);
            // Allow a grace period when checking for start dates in the past.
            if (validFrom.has_value())
            {
                ErpExpect(std::chrono::system_clock::time_point{std::chrono::seconds{*validFrom}} >=
                              (std::chrono::system_clock::now() - 5min),
                          HttpStatus::BadRequest, "timestamp lies in the past");

                entry.startDateTime = std::chrono::system_clock::time_point{std::chrono::seconds{*validFrom}};
            }
            else
            {
                entry.startDateTime = std::chrono::system_clock::now();
            }
        }
        catch (const ErpException& exception)
        {
            session.accessLog.error("caught ErpException in PostVauAut::doHandleRequest");
            TVLOG(1) << "exception details: " << exception.what();

            session.response.setStatus(::HttpStatus::BadRequest);
            EnrolmentModel response;
            response.set("/code", "START_NOT_VALID");
            response.set("/description", "The value for valid-from is invalid or in the past.");
            return response;
        }

        session.baseServiceContext.getBlobCache()->storeBlob(std::move(entry));

        session.response.setStatus(HttpStatus::Created);

        return {};
    }
    catch (const ErpException& exception)
    {
        if (exception.status() == HttpStatus::BadRequest)
        {
            session.accessLog.error("caught ErpException in PostAutSig::doHandleRequest");
            TVLOG(1) << "exception details: " << exception.what();

            session.response.setStatus(exception.status());
            EnrolmentModel response;
            response.set("/code", "INVALID");
            response.set("/description", "The input could not be parsed or input validation failed.");
            return response;
        }
        else
        {
            throw;
        }
    }
}

DeleteVauAut::DeleteVauAut()
    : DeleteBlobHandler(BlobType::VauAut, "/Enrolment/VauAut")
{
}

EnrolmentModel PostVsdmHmacKey::doHandleRequest(EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("POST /Enrolment/VsdmHmacKey");

    try
    {
        EnrolmentModel requestData(session.request.getBody());
        auto hsmPoolSession = session.baseServiceContext.getHsmPool().acquire();
        auto& hsmSession = hsmPoolSession.session();

        // extract the full package from the json request and validate the parameters
        // note that we ignore the expiry field
        const auto blobGeneration =
            gsl::narrow_cast<ErpBlob::GenerationId>(requestData.getInt64(requestBlobGeneration));
        auto vsdmBlobData = requestData.getDecodedString(requestBlobData);
        ErpBlob vsdmBlob{std::move(vsdmBlobData), blobGeneration};

        ErpVector unwrappedEntry = hsmSession.unwrapRawPayload(vsdmBlob);
        std::string_view vsdmPackage{reinterpret_cast<const char*>(unwrappedEntry.data()), unwrappedEntry.size()};

        // GEMREQ-start A-27299#store
        A_23492.start("decrypt vsdm key and store it");
        VsdmHmacKey storedVsdmPackage{vsdmPackage};
        // get and validate the passed hmac key by testing it
        const auto plainTextKey = storedVsdmPackage.decryptHmacKey(hsmSession, true);
        // remove the encrypted_key, we dont need it anymore
        storedVsdmPackage.deleteKeyInformation();
        // store the plaintext key
        storedVsdmPackage.setPlainTextKey(Base64::encode(plainTextKey));

        const auto serializedVsdmKeyData = storedVsdmPackage.serializeToString();
        const ErpVector vsdmKeyData = ErpVector::create(serializedVsdmKeyData);

        VsdmKeyBlobDatabase::Entry vsdmKeyBlobEntry;
        vsdmKeyBlobEntry.operatorId = storedVsdmPackage.operatorId();
        vsdmKeyBlobEntry.version = storedVsdmPackage.version();
        vsdmKeyBlobEntry.blob = hsmSession.wrapRawPayload(vsdmKeyData, blobGeneration);
        vsdmKeyBlobEntry.createdDateTime = std::chrono::system_clock::now();
        session.baseServiceContext.getVsdmKeyBlobDatabase().storeBlob(std::move(vsdmKeyBlobEntry));
        A_23492.finish();
        // GEMREQ-end A-27299#store
        session.response.setStatus(HttpStatus::Created);
    }
    catch (const ErpException& exception)
    {
        session.response.setStatus(exception.status());
        if (exception.status() == HttpStatus::BadRequest)
        {
            session.accessLog.error("caught ErpException in PostVsdmHmacKey::doHandleRequest");
            TVLOG(1) << "exception details: " << exception.what();

            EnrolmentModel response;
            response.set("/code", "INVALID");
            response.set("/description", exception.what());
            return response;
        }
        else
        {
            throw;
        }
    }
    return {};
}

EnrolmentModel GetVsdmHmacKey::doHandleRequest(EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("GET /Enrolment/VsdmHmacKey");

    try
    {
        auto hsmPoolSession = session.baseServiceContext.getHsmPool().acquire();
        auto& hsmSession = hsmPoolSession.session();

        std::vector<VsdmKeyBlobDatabase::Entry> blobEntries =
            session.baseServiceContext.getVsdmKeyBlobDatabase().getAllBlobs();
        // unwrap all the keypackages and return them without the key themselves
        EnrolmentModel response;
        auto& responseDocument = response.document();
        responseDocument.SetArray();
        for (const auto& entry : blobEntries)
        {
            // unwrap each entry so we can access the plaintext key and recalculate the
            // hmac against the empty string
            ErpVector unwrappedEntry = hsmSession.unwrapRawPayload(entry.blob);
            std::string_view payload{reinterpret_cast<const char*>(unwrappedEntry.data()), unwrappedEntry.size()};
            VsdmHmacKey storedVsdmKeyData{payload};
            const auto hmacEmptyString = Hash::hmacSha256(Base64::decode(storedVsdmKeyData.plainTextKey()), "");
            auto hmacEmptyStringHex = ByteHelper::toHex(hmacEmptyString);
            auto exp = storedVsdmKeyData.getOptionalString(VsdmHmacKey::jsonKey::exp);

            A_23486.start("remove key data from output packages");
            // create a new package which does not contain any other information than explicitly set
            VsdmHmacKey returnedVsdmPackge{storedVsdmKeyData.operatorId(), storedVsdmKeyData.version()};
            returnedVsdmPackge.set("/generation", entry.blob.generation);
            auto timestamp = model::Timestamp(entry.createdDateTime);
            returnedVsdmPackge.set("/created", timestamp.toXsDateTimeWithoutFractionalSeconds());
            returnedVsdmPackge.set(VsdmHmacKey::jsonKey::hmacEmptyStringCalculated, hmacEmptyStringHex);
            returnedVsdmPackge.set(VsdmHmacKey::jsonKey::hmacEmptyString,
                                   storedVsdmKeyData.getString(VsdmHmacKey::jsonKey::hmacEmptyString));
            if (exp)
            {
                returnedVsdmPackge.set(VsdmHmacKey::jsonKey::exp, *exp);
            }
            A_23486.finish();
            rapidjson::Value dstVal;
            dstVal.CopyFrom(returnedVsdmPackge.document(), responseDocument.GetAllocator());

            responseDocument.PushBack(dstVal, responseDocument.GetAllocator());
        }
        return response;
    }
    catch (const ErpException& exception)
    {
        session.response.setStatus(exception.status());
        if (exception.status() == HttpStatus::BadRequest)
        {
            session.accessLog.error("caught ErpException in GetVsdmHmacKey::doHandleRequest");
            TVLOG(1) << "exception details: " << exception.what();

            EnrolmentModel response;
            response.set("/code", "INVALID");
            response.set("/description", exception.what());
            return response;
        }
        else
        {
            throw;
        }
    }
    return {};
}


EnrolmentModel DeleteVsdmHmacKey::doHandleRequest(EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("DELETE /Enrolment/VsdmHmacKey");

    try
    {
        EnrolmentModel requestData(session.request.getBody());
        const std::string vsdmPackage = requestData.serializeToString(requestVsdm);
        const VsdmHmacKey key = VsdmHmacKey{vsdmPackage};

        session.baseServiceContext.getVsdmKeyBlobDatabase().deleteBlob(key.operatorId(), key.version());
        session.response.setStatus(HttpStatus::NoContent);
    }
    catch (const ErpException& exception)
    {
        session.response.setStatus(exception.status());
        if (exception.status() == HttpStatus::BadRequest)
        {
            session.accessLog.error("caught ErpException in DeleteVsdmHmacKey::doHandleRequest");
            TVLOG(1) << "exception details: " << exception.what();

            EnrolmentModel response;
            response.set("/code", "INVALID");
            response.set("/description", exception.what());
            return response;
        }
        else
        {
            throw;
        }
    }
    return {};
}


} // end of namespace enrolment

/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/enrolment/EnrolmentRequestHandler.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/hsm/ErpTypes.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/tpm/PcrSet.hxx"
#include "erp/tpm/Tpm.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Hash.hxx"
#include "erp/util/TLog.hxx"

#if WITH_HSM_TPM_PRODUCTION > 0
#include "erp/enrolment/EnrolmentHelper.hxx"
#endif

using namespace ::std::literals;

namespace
{

    /**
     * Acquire a TPM instance on demand so that the TPM is not blocked longer than necessary.
     */
    std::unique_ptr<Tpm> getTpm (EnrolmentSession& session)
    {
        auto tpm = session.serviceContext.getTpmFactory()(session.serviceContext.getBlobCache());
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
            const auto expectedPcrSet = PcrSet::fromString(Configuration::instance().getOptionalStringValue(
                ConfigurationKey::PCR_SET,
                PcrSet::defaultSet().toString(false)));
            const auto givenPcrSet = PcrSet::fromList(inputPcrSet);
            ErpExpect(givenPcrSet == expectedPcrSet, HttpStatus::BadRequest,
                "given pcr set " + givenPcrSet.toString() + " is the same as the expected " + expectedPcrSet.toString());
        }
        catch (const std::runtime_error& e)
        {
            TLOG(WARNING) << "caught exception '" << e.what() << "' while verifying pcr set, converting to BadRequest";
            ErpFail(HttpStatus::BadRequest, e.what());
        }
    }

::std::string getBase64PcrHash(::std::string_view base64Quote, ::PcServiceContext& serviceContext)
{
    auto hsmPoolSession = serviceContext.getHsmPool().acquire();
    const auto quoteData = ::ErpVector{::Base64::decode(base64Quote)};
    const auto pcrHash = hsmPoolSession.session().parseQuote(quoteData).pcrHash;
    return ::Base64::encode(pcrHash);
}

    bool isCertificateValid(const std::string& certificate, TslManager& tslManager)
    {
        try
        {
            // TODO: get rid of the back and forth once X509Certificate is merged with Certificate
            const auto base64DerCertificate = Certificate::fromBase64(certificate).toBase64Der();
            auto x509Certificate = X509Certificate::createFromBase64(base64DerCertificate);
            tslManager.verifyCertificate(TslMode::TSL, x509Certificate, {CertificateType::C_FD_OSIG});

            return true;
        }
        catch (const std::exception& exception)
        {
            TVLOG(1) << "unable to check certificate received in PostVauSig: " << exception.what();
            return false;
        }
    }
}

enum ResourceType
{
    Task,
    Communication,
    AuditLog
};


BlobType getBlobTypeForResourceType (const ResourceType resourceType)
{
    switch(resourceType)
    {
        case Task: return BlobType::TaskKeyDerivation;
        case Communication: return BlobType::CommunicationKeyDerivation;
        case AuditLog: return BlobType::AuditLogKeyDerivation;
        default: Fail("unsupported resource type");
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
    session.serviceContext.getBlobCache().storeBlob(std::move(entry));

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

    session.serviceContext.getBlobCache().deleteBlob(mBlobType, blobId);

    session.response.setStatus(HttpStatus::NoContent);
    return {};
}




namespace enrolment
{

EnrolmentModel GetEnclaveStatus::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("GET /Enrolment/EnclaveStatus");
    ErpExpect(session.request.getBody().empty(), HttpStatus::BadRequest, "no request body expected");

    const auto enrolmentStatus = EnrolmentData::getEnclaveStatus(session.serviceContext.getBlobCache());

    EnrolmentModel responseData;
    responseData.set(responseEnclaveId, session.serviceContext.getEnrolmentData().enclaveId.toString());
    responseData.set(responseEnclaveTime, std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    responseData.set(responseEnrolmentStatus, magic_enum::enum_name(enrolmentStatus));
    const auto pcrSet = ::PcrSet::fromString(::Configuration::instance().getOptionalStringValue(
        ::ConfigurationKey::PCR_SET, ::PcrSet::defaultSet().toString(false)));
    responseData.set(responsePcrSet, pcrSet.toString());

    auto hsmPoolSession = session.serviceContext.getHsmPool().acquire();
    const auto nonce = ::Base64::encode(hsmPoolSession.session().getNonce(0).nonce);
    const auto quote =
        getTpm(session)->getQuote(::Tpm::QuoteInput{nonce, pcrSet.toPcrList(), ::std::string{hashAlgorithm}}, true);
    responseData.set(responsePcrHash, getBase64PcrHash(quote.quotedDataBase64, session.serviceContext));

    responseData.set(responseVersionRelease, ErpServerInfo::ReleaseVersion);
    responseData.set(responseVersionReleasedate, ErpServerInfo::ReleaseDate);
    responseData.set(responseVersionBuild, ErpServerInfo::BuildVersion);
    responseData.set(responseVersionBuildType, ErpServerInfo::BuildType);
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
    responseData.set(responsePcrHash, getBase64PcrHash(output.quotedDataBase64, session.serviceContext));

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

    session.serviceContext.getBlobCache().storeBlob(std::move(entry));

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
        pcrSet = ::PcrSet::fromString(::Configuration::instance().getOptionalStringValue(
                                          ::ConfigurationKey::PCR_SET, ::PcrSet::defaultSet().toString(false)))
                     .toPcrList();
    }

    ::BlobDatabase::Entry entry;
    entry.type = ::BlobType::Quote;
    entry.name = requestData.getDecodedErpVector(requestId);
    const auto blobGeneration =
        ::gsl::narrow_cast<::ErpBlob::GenerationId>(requestData.getInt64(requestBlobGeneration));
    entry.blob = ::ErpBlob{requestData.getDecodedString(requestBlobData), blobGeneration};
    entry.pcrSet = ::PcrSet::fromList(pcrSet.value());

    auto hsmPoolSession = session.serviceContext.getHsmPool().acquire();
    const auto nonce = ::Base64::encode(hsmPoolSession.session().getNonce(0).nonce);
    const auto quote =
        getTpm(session)->getQuote(::Tpm::QuoteInput{nonce, pcrSet.value(), ::std::string{hashAlgorithm}}, true);
    entry.pcrHash = ::ErpVector{::Base64::decode(getBase64PcrHash(quote.quotedDataBase64, session.serviceContext))};
    auto& blobCache = session.serviceContext.getBlobCache();

#if WITH_HSM_TPM_PRODUCTION > 0
    try
    {
        ::EnrolmentHelper{HsmIdentity::getWorkIdentity()}.createTeeToken(blobCache, entry);
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
    session.serviceContext.getBlobCache().storeBlob(std::move(entry));

    session.response.setStatus(HttpStatus::Created);
    return {};
}


EnrolmentModel DeleteEciesKeypair::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("DELETE /Enrolment/EciesKeypair");

    EnrolmentModel requestData (session.request.getBody());
    auto blobId = requestData.getDecodedErpVector(requestId);

    session.serviceContext.getBlobCache().deleteBlob(BlobType::EciesKeypair, blobId);

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
    session.serviceContext.getBlobCache().storeBlob(std::move(entry));

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

    session.serviceContext.getBlobCache().deleteBlob(blobType, blobId);

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

        entry.certificate = requestData.getDecodedString(requestCertificate);

        auto& tslManager = session.serviceContext.getTslManager();
        if (!isCertificateValid(*entry.certificate, tslManager))
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
        catch (ErpException& exception)
        {
            session.accessLog.error("caught ErpException in PutVauSig::doHandleRequest");
            TVLOG(1) << "exception details: " << exception.what();

            session.response.setStatus(::HttpStatus::BadRequest);
            ::EnrolmentModel response;
            response.set("/code", "START_NOT_VALID");
            response.set("/description", "The value for valid-from is invalid or in the past.");
            return response;
        }

        session.serviceContext.getBlobCache().storeBlob(std::move(entry));

        session.response.setStatus(HttpStatus::Created);

        return {};
    }
    catch (ErpException& exception)
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


} // end of namespace enrolment

#include "erp/enrolment/EnrolmentRequestHandler.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/tpm/PcrSet.hxx"
#include "erp/tpm/Tpm.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Hash.hxx"
#include "erp/util/TLog.hxx"


namespace
{
    /**
     * Acquire a TPM instance on demand so that the TPM is not blocked longer than necessary.
     */
    std::unique_ptr<Tpm> getTpm (EnrolmentSession& session)
    {
        auto tpm = session.serviceContext.tpmFactory(*session.serviceContext.blobCache);
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
        handleBasicAuthentication(session);

        const auto document = doHandleRequest(session);
        session.response.setBody(document.serializeToString());
        if (session.response.getHeader().status() == HttpStatus::Unknown)
            session.response.setStatus(HttpStatus::OK);
        // else don't overwrite if status has already been set explicitly.
        session.response.setHeader(Header::ContentType, MimeType::json);
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


void EnrolmentRequestHandlerBase::handleBasicAuthentication(const EnrolmentSession& session)
{
    if (Configuration::instance().getOptionalBoolValue(ConfigurationKey::DEBUG_DISABLE_ENROLMENT_API_AUTH, false) == false)
    {
        SafeString secret(Configuration::instance().getSafeStringValue(ConfigurationKey::ENROLMENT_API_CREDENTIALS));
        ErpExpect(session.request.header().hasHeader(Header::Authorization), HttpStatus::Unauthorized, "Authorization required.");
        auto parts = String::split(session.request.header().header(Header::Authorization).value_or(""), " ");
        if (parts.size() == 2)
        {
            SafeString authSecret(std::move(parts[1]));
            ErpExpect(String::toLower(parts[0]) == "basic" && authSecret == secret, HttpStatus::Unauthorized, "Unauthorized");
        }
        else
        {
            ErpExpect(false, HttpStatus::Unauthorized, "Unauthorized (unsupported authorization content.)");
        }
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
    session.serviceContext.blobCache->storeBlob(std::move(entry));

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

    session.serviceContext.blobCache->deleteBlob(mBlobType, blobId);

    session.response.setStatus(HttpStatus::NoContent);
    return {};
}




namespace enrolment
{

EnrolmentModel GetEnclaveStatus::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("GET /Enrolment/EnclaveStatus");
    ErpExpect(session.request.getBody().empty(), HttpStatus::BadRequest, "no request body expected");

    const auto enrolmentStatus = EnrolmentData::getEnclaveStatus(*session.serviceContext.blobCache);

    EnrolmentModel responseData;
    responseData.set(responseEnclaveId, session.serviceContext.enrolmentData.enclaveId.toString());
    responseData.set(responseEnclaveTime, std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    responseData.set(responseEnrolmentStatus, magic_enum::enum_name(enrolmentStatus));
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

    session.serviceContext.blobCache->storeBlob(std::move(entry));

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

    EnrolmentModel requestData (session.request.getBody());
    auto blobId = requestData.getDecodedErpVector(requestId);
    auto blobData = requestData.getDecodedString(requestBlobData);
    auto optionalPcrSet = requestData.getOptionalSizeTVector(requestPcrSet);

    const auto blobGeneration = requestData.getInt64(requestBlobGeneration);

    BlobDatabase::Entry entry;
    entry.type = BlobType::Quote;
    entry.name = blobId;
    entry.blob = ErpBlob(std::move(blobData), gsl::narrow_cast<ErpBlob::GenerationId>(blobGeneration));

    // The pcrSet field will soon not be optional anymore.
    // For a transitioning phase use the default set as a fall back.
    if (optionalPcrSet.has_value())
        entry.metaPcrSet = PcrSet::fromList(optionalPcrSet.value());
    else
        entry.metaPcrSet = PcrSet::defaultSet();

    session.serviceContext.blobCache->storeBlob(std::move(entry));

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
    session.serviceContext.blobCache->storeBlob(std::move(entry));

    session.response.setStatus(HttpStatus::Created);
    return {};
}


EnrolmentModel DeleteEciesKeypair::doHandleRequest (EnrolmentSession& session)
{
    session.accessLog.setInnerRequestOperation("DELETE /Enrolment/EciesKeypair");

    EnrolmentModel requestData (session.request.getBody());
    auto blobId = requestData.getDecodedErpVector(requestId);

    session.serviceContext.blobCache->deleteBlob(BlobType::EciesKeypair, blobId);

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
    const auto startDateTime = requestData.getInt64(requestStartDateTime);
    const auto endDateTime = requestData.getInt64(requestEndDateTime);

    BlobDatabase::Entry entry;
    entry.type = getBlobTypeForResourceType(resourceType.value());
    entry.name = blobId;
    entry.blob = ErpBlob(std::move(blobData), gsl::narrow_cast<ErpBlob::GenerationId>(blobGeneration));
    entry.startDateTime = std::chrono::system_clock::time_point(std::chrono::seconds(startDateTime));
    entry.endDateTime = std::chrono::system_clock::time_point(std::chrono::seconds(endDateTime));
    session.serviceContext.blobCache->storeBlob(std::move(entry));

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

    session.serviceContext.blobCache->deleteBlob(blobType, blobId);

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


PutVauSigPrivateKey::PutVauSigPrivateKey (void)
    : PutBlobHandler(BlobType::VauSigPrivateKey, "/Enrolment/VauSigPrivateKey")
{
}


DeleteVauSigPrivateKey::DeleteVauSigPrivateKey (void)
    : DeleteBlobHandler(BlobType::VauSigPrivateKey, "/Enrolment/VauSigPrivateKey")
{
}


} // end of namespace enrolment

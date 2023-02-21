/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/enrolment/EnrolmentHelper.hxx"
#include "erp/enrolment/EnrolmentModel.hxx"
#include "erp/common/Constants.hxx"
#include "erp/hsm/production/HsmProductionClient.hxx"
#include "erp/hsm/production/HsmRawSession.hxx"
#if !defined __APPLE__ && !defined _WIN32
#include "erp/hsm/HsmIdentity.hxx"
#include "erp/tpm/TpmProduction.hxx"
#else
#include "mock/tpm/TpmMock.hxx"
#endif
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/DurationConsumer.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Hash.hxx"



namespace
{
    #define catchLogAndRethrow(message)                                                       \
        catch(const std::exception& e)                                                        \
        {                                                                                     \
            TVLOG(logLevel) << "caught exception " << e.what() << " while " << (message);     \
            throw;                                                                            \
        }                                                                                     \
        catch(...)                                                                            \
        {                                                                                     \
            TVLOG(logLevel) << "caught exception while " << (message);                        \
            throw;                                                                            \
        }

    ErpBlob convertBlob (hsmclient::ERPBlob& input)
    {
        ErpBlob output;
        output.generation = input.BlobGeneration;
        output.data = SafeString(input.BlobData, input.BlobLength);
        return output;
    }

    void writeBlob (const ErpBlob& source, hsmclient::ERPBlob& destination)
    {
        destination.BlobGeneration = source.generation;
        destination.BlobLength = source.data.size();
        const auto* p = static_cast<const char*>(source.data);
        Expect(source.data.size()<=MAX_BUFFER, "data does not fit inside an HSM blob");
        std::copy(p, p+source.data.size(), destination.BlobData);
    }

    void writeVector (const std::vector<uint8_t>& source, uint8_t* destinationBuffer, size_t& destinationSize)
    {
        destinationSize = source.size();
        Expect(source.size() <= MAX_BUFFER, "vector is too large");
        std::copy(source.begin(), source.end(), destinationBuffer);
    }

    void writeVector (const std::string& source, uint8_t* destinationBuffer, size_t& destinationSize)
    {
        destinationSize = source.size();
        Expect(source.size() <= MAX_BUFFER, "vector is too large");
        std::copy(source.begin(), source.end(), destinationBuffer);
    }

    template<size_t N>
    void writeArray (const std::array<uint8_t,N>& source, uint8_t* destinationBuffer)
    {
        std::copy(source.begin(), source.end(), destinationBuffer);
    }


    ErpArray<TpmObjectNameLength> decodeAkName (const std::string& base64)
    {
        ErpArray<TpmObjectNameLength> array;
        auto decoded = Base64::decode(base64);
        Expect(decoded.size() == TpmObjectNameLength, "decoded ak name has wrong length");
        std::copy(decoded.begin(), decoded.end(), array.begin());
        return array;
    }


    util::Buffer toBufferWithoutAsn1Header (const std::vector<uint8_t>& data)
    {
        // Cut off the first two bytes which are part of the ASN1 container.
        Expect(data.size() >= 2, "vector too small for an ASN1 container");
        return util::rawToBuffer(data.data()+2, data.size()-2);
    }


    std::vector<uint8_t> hmac (const std::vector<uint8_t>& key, const std::string& message)
    {
        return Hash::hmacSha256(key, message);
    }

    PcrSet getConfiguredOrDefaultPcrSet (void)
    {
        return PcrSet::fromString(
            // Use the configured value ...
            Configuration::instance().getOptionalStringValue(
                ConfigurationKey::PCR_SET,
                // ... and fall back to the default
                PcrSet::defaultSet().toString(false)));
    }

} // end of anonymous namespace

TpmProxyDirect::TpmProxyDirect (BlobCache& blobCache)
    : mBlobCache(blobCache)
{
}


Tpm::EndorsementKeyOutput TpmProxyDirect::getEndorsementKey (void)
{
#if !defined __APPLE__ && !defined _WIN32
    return TpmProduction::createInstance(mBlobCache)->getEndorsementKey();
#else
    return TpmMock().getEndorsementKey();
#endif
}


Tpm::AttestationKeyOutput TpmProxyDirect::getAttestationKey (bool isEnrolmentActive)
{
#if !defined __APPLE__ && !defined _WIN32
    return TpmProduction::createInstance(mBlobCache)->getAttestationKey(isEnrolmentActive);
#else
    return TpmMock().getAttestationKey(isEnrolmentActive);
#endif
}


Tpm::AuthenticateCredentialOutput TpmProxyDirect::authenticateCredential (Tpm::AuthenticateCredentialInput&& input)
{
#if !defined __APPLE__ && !defined _WIN32
    return TpmProduction::createInstance(mBlobCache)->authenticateCredential(std::move(input));
#else
    return TpmMock().authenticateCredential(std::move(input));
#endif
}


Tpm::QuoteOutput TpmProxyDirect::getQuote (Tpm::QuoteInput&& input, const std::string& message)
{
    const auto nonce = Base64::decode(input.nonceBase64);
    input.nonceBase64 = Base64::encode(hmac(nonce, message));
#if !defined __APPLE__ && !defined _WIN32
    return TpmProduction::createInstance(mBlobCache)->getQuote(std::move(input));
#else
    return TpmMock().getQuote(std::move(input));
#endif
}


ErpVector EnrolmentHelper::getBlobName (const ErpBlob& blob)
{
    const auto hash = Hash::sha256(blob.data);
    return ErpVector(hash.begin(), hash.end());
}



/**
 * Another class in order to hide the use of header files and data structures that are only available on Linux.
 */
EnrolmentHelper::EnrolmentHelper (
    const HsmIdentity& identity,
    const std::string& certificateFilename,
    const int32_t _logLevel)
    : logLevel(_logLevel),
      mCertificateFilename(certificateFilename),
      mHsmSession(HsmProductionClient::connect(identity))
{
}


EnrolmentHelper::~EnrolmentHelper (void)
{
    HsmProductionClient::disconnect(mHsmSession);
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
EnrolmentHelper::Blobs EnrolmentHelper::createBlobs (
    TpmProxy& tpm)
{
    // The attestation process that is implemented in this method is not used in production. However it shares enough
    // code with the production code to be not easily separable. Therefore it remains here for the moment.
    const uint32_t generation = 0x42; // Arbitrary generation value, copied over from the original code [Sic, hex 42, not decimal 42].
    TVLOG(1) << "creating blobs 1/10: get PCR set";
    const auto pcrSet = getConfiguredOrDefaultPcrSet();

    TVLOG(1) << "creating blobs 2/11: trustTpmMfr";
    const auto trustedRoot = trustTpmMfr(generation);
    TVLOG(1) << "creating blobs 3/11: getEndorsementKey";
    const auto ek = tpm.getEndorsementKey();
    TVLOG(1) << "creating blobs 4/11: getAttestationKey";
    const auto ak = tpm.getAttestationKey(true);
    const auto akName = decodeAkName(ak.akNameBase64);
    const auto akPublicKey = Base64::decodeToString(ak.publicKeyBase64);
    TVLOG(1) << "creating blobs 5/11: enrollEk";
          auto trustedEk = enrollEk(generation, trustedRoot, Base64::decode(ek.certificateBase64));
    TVLOG(1) << "creating blobs 6/11: getAkChallenge";
    const auto [akChallenge, credential, secret] = getAkChallenge(generation, trustedEk, akPublicKey, akName);
    TVLOG(1) << "creating blobs 7/11: getChallengeAnswer";
    const auto decCredential = getChallengeAnswer(tpm, secret, credential, akName);
    TVLOG(1) << "creating blobs 8/11: enrollAk";
          auto trustedAk = enrollAk(generation, trustedEk, akChallenge, akPublicKey, akName, decCredential);
    TVLOG(1) << "creating blobs 9/11: getNonce";
    const auto [nonceVector, nonceBlob] = getNonce(generation);
    TVLOG(1) << "creating blobs 10/11: getQuote";
    const auto quote = getQuote(tpm, nonceVector, pcrSet.toPcrList(), "ERP_ENROLLMENT");
    TVLOG(1) << "creating blobs 11/11: enrollEnclave";
          auto trustedQuote = enrollEnclave(generation, akName, quote, trustedAk, nonceBlob);
    TVLOG(1) << "creating blobs: successfully finished";

    EnrolmentHelper::Blobs blobs;
    blobs.akName = ErpVector(akName.begin(), akName.end());
    blobs.trustedAk = std::move(trustedAk);
    blobs.trustedEk = std::move(trustedEk);
    blobs.trustedQuote = std::move(trustedQuote);
    blobs.pcrSet = pcrSet;
    return blobs;
}


ErpBlob EnrolmentHelper::createTeeToken(BlobCache& blobCache, ::std::optional<::BlobCache::Entry> trustedQuote)
{
    const uint32_t teeGeneration = 0;

    TpmProxyDirect tpm (blobCache);

    const auto [teeTokenNonceVector, teeTokenNonceBlob] = getNonce(teeGeneration);

    const auto teeQuote =
        getQuote(tpm, teeTokenNonceVector, getConfiguredOrDefaultPcrSet().toPcrList(), "ERP_ATTESTATION");

    const auto quote = ::ErpVector{::Base64::decode(teeQuote.quotedDataBase64)};
    blobCache.setPcrHash(::HsmProductionClient{}.parseQuote(quote).pcrHash);
    if (! trustedQuote)
    {
        trustedQuote = blobCache.getBlob(::BlobType::Quote);
    }

    const auto trustedAk = blobCache.getBlob(BlobType::AttestationPublicKey);
    return getTeeToken(trustedAk.getAkName(), teeQuote, trustedAk.blob, teeTokenNonceBlob, trustedQuote->blob);
}


ErpBlob EnrolmentHelper::trustTpmMfr (const uint32_t generation)
{
    auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryEnrolmentHelper,
                                                         "EnrolmentHelper:ERP_TrustTPMMfr");

    hsmclient::TrustTPMMfrInput input;

    input.desiredGeneration = generation;

    {
        // Load cacertecc.crt and set it as input value.
        const auto certificateString = FileHelper::readFileAsString(mCertificateFilename);
        Expect(certificateString.size() <= MAX_BUFFER, "content of cacertecc.crt is too long to fit into input of ERP_TrustTPMMfr");
        input.certLength = certificateString.size();
        std::copy(certificateString.begin(), certificateString.end(), input.certData);
    }

    hsmclient::SingleBlobOutput output = ERP_TrustTPMMfr(mHsmSession.rawSession, input);
    HsmExpectSuccess(output, "ERP_TrustTPMMfr failed", timer);
    TVLOG(logLevel) << "successfully called ERP_TrustTPMMfr and got blob of size " << output.BlobOut.BlobLength << " in return";

    return convertBlob(output.BlobOut);
}


ErpBlob EnrolmentHelper::enrollEk (const uint32_t generation, const ErpBlob& trustedRoot, const std::vector<uint8_t>& ekCertificate)
{
    auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryEnrolmentHelper,
                                                         "EnrolmentHelper:ERP_EnrollTPMEK");

    hsmclient::EnrollTPMEKInput input;
    input.desiredGeneration = generation;
    writeBlob(trustedRoot, input.TPMMfrBlob);
    writeVector(ekCertificate, input.EKCertData, input.EKCertLength);

    auto output = ERP_EnrollTPMEK(mHsmSession.rawSession, input);

    HsmExpectSuccess(output, "ERP_EnrollTPMEK failed", timer);
    TVLOG(logLevel) << "successfully called ERP_EnrollTPMEK and got blob of size " << output.BlobOut.BlobLength << " in return";

    return convertBlob(output.BlobOut);
}


std::tuple<ErpBlob, // Challenge blob
    std::vector<uint8_t>, // Credential data
    std::vector<uint8_t>> // Secret
EnrolmentHelper::getAkChallenge (
    const uint32_t generation,
    const ErpBlob& trustedEk,
    const std::string& akPublicKey,
    const ErpArray<TpmObjectNameLength>& akName)
{
    auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryEnrolmentHelper,
                                                         "EnrolmentHelper:ERP_GetAKChallenge");

    // For some reason the public key in the attestation key data blob contains two leading bytes that
    // are not to be transferred to the following HSM call. Remove them.
    std::vector<uint8_t> publicKey (akPublicKey.begin()+2, akPublicKey.end());

    hsmclient::AKChallengeInput input;
    input.desiredGeneration = generation;
    writeBlob(trustedEk, input.KnownEKBlob);
    writeArray<TPM_NAME_LEN>(akName, input.AKName);
    writeVector(publicKey, input.AKPubData, input.AKPubLength);

    auto output = ERP_GetAKChallenge(mHsmSession.rawSession, input);

    HsmExpectSuccess(output, "ERP_GetAKChallenge failed", timer);
    TVLOG(logLevel) << "successfully called ERP_GetAKChallenge and got blob of size " << output.ChallengeBlob.BlobLength << " in return";

    return {
        convertBlob(output.ChallengeBlob),
        std::vector<uint8_t>(output.encCredentialData, output.encCredentialData + output.encCredentialLength),
        std::vector<uint8_t>(output.secretData, output.secretData + output.secretLength)
    };
}


std::string EnrolmentHelper::getChallengeAnswer (
    TpmProxy& tpm,
    const std::vector<uint8_t>& secret,
    const std::vector<uint8_t>& encryptedCredential,
    const ErpArray<TpmObjectNameLength>& akName)
{
    try
    {
        Tpm::AuthenticateCredentialInput input;
        input.secretBase64 = Base64::encode(toBufferWithoutAsn1Header(secret));
        input.credentialBase64 = Base64::encode(toBufferWithoutAsn1Header(encryptedCredential));
        input.akNameBase64 = Base64::encode(util::rawToBuffer(akName.data(), akName.size()));

        auto credential = tpm.authenticateCredential(std::move(input));

        TVLOG(logLevel) << "authenticateCredential\n"
                 << "secret " << Base64::encode(toBufferWithoutAsn1Header(secret)) << '\n'
                 << "credential " << Base64::encode(toBufferWithoutAsn1Header(encryptedCredential)) << '\n'
                 << "akname " << Base64::encode(util::rawToBuffer(akName.data(), akName.size())) << '\n'
                 << "result " << credential.plainTextCredentialBase64;

        return Base64::decodeToString(credential.plainTextCredentialBase64);
    }
    catchLogAndRethrow("calling authenticateCredential");
}


ErpBlob EnrolmentHelper::enrollAk (
    const uint32_t generation,
    const ErpBlob& trustedEk,
    const ErpBlob& akChallenge,
    const std::string& akPublicKey,
    const ErpArray<TpmObjectNameLength>& akName,
    const std::string& decCredential)
{
    auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryEnrolmentHelper,
                                                         "EnrolmentHelper:ERP_EnrollTPMAK");

    // Remove two bytes of the ASN1 container.
    Expect(akPublicKey.size() >= 2, "akPublicKey too small to be an ASN1 container");
    std::vector<uint8_t> publicKey (akPublicKey.begin()+2, akPublicKey.end());

    hsmclient::EnrollTPMAKInput input;
    input.desiredGeneration = generation;
    writeBlob(trustedEk, input.KnownEKBlob);
    writeArray<TPM_NAME_LEN>(akName, input.AKName);
    writeVector(publicKey, input.AKPubData, input.AKPubLength);
    writeBlob(akChallenge, input.challengeBlob);
    writeVector(decCredential, input.decCredentialData, input.decCredentialLength);

    auto output = ERP_EnrollTPMAK(mHsmSession.rawSession, input);

    HsmExpectSuccess(output, "ERP_EnrollTPMAK failed", timer);
    TVLOG(logLevel) << "successfully called ERP_EnrollTPMAK and got blob of size " << output.BlobOut.BlobLength << " in return";

    return convertBlob(output.BlobOut);
}


std::tuple<
    std::vector<uint8_t>,
    ErpBlob> EnrolmentHelper::getNonce (const uint32_t generation)
{
    auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryEnrolmentHelper,
                                                         "EnrolmentHelper:ERP_GenerateNONCE");

    hsmclient::UIntInput input;
    input.intValue = generation;

    auto output = ERP_GenerateNONCE(mHsmSession.rawSession, input);

    HsmExpectSuccess(output, "ERP_GenerateNONCE failed", timer);
    TVLOG(logLevel) << "successfully called ERP_GenerateNONCE and got nonce of size " << NONCE_LEN;

    return {
        std::vector<uint8_t>(output.NONCE, output.NONCE+NONCE_LEN),
        convertBlob(output.BlobOut)
    };
}


Tpm::QuoteOutput EnrolmentHelper::getQuote (
    TpmProxy& tpm,
    const std::vector<uint8_t>& nonce,
    const Tpm::PcrRegisterList& pcrSet,
    const std::string& message)
{
    try
    {
        Tpm::QuoteInput input;
        input.nonceBase64 = Base64::encode(nonce);
        input.pcrSet = pcrSet;
        input.hashAlgorithm = "SHA256";

        auto quote = tpm.getQuote(std::move(input), message);

        TVLOG(logLevel) << "successfully called TPM getQuote";

        return quote;
    }
    catchLogAndRethrow("calling getQuote");
}


ErpBlob EnrolmentHelper::enrollEnclave (
    const uint32_t generation,
    const ErpArray<TpmObjectNameLength>& akName,
    const Tpm::QuoteOutput& quote,
    const ErpBlob& knownAk,
    const ErpBlob& nonce)
{
    auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryEnrolmentHelper,
                                                         "EnrolmentHelper:ERP_EnrolEnclave");

    hsmclient::EnrollEnclaveInput input;
    input.desiredGeneration = generation;
    writeArray(akName, input.AKName);
    writeVector(Base64::decode(quote.quotedDataBase64), input.quoteData, input.quoteLength);
    writeVector(Base64::decode(quote.quoteSignatureBase64), input.signatureData, input.signatureLength);
    writeBlob(knownAk, input.KnownAKBlob);
    writeBlob(nonce, input.NONCEBlob);

    auto output = ERP_EnrollEnclave(mHsmSession.rawSession, input);

    HsmExpectSuccess(output, "ERP_EnrollEnclave failed", timer);
    TVLOG(logLevel) << "successfully called ERP_EnrollEnclave";

    return convertBlob(output.BlobOut);
}


ErpBlob EnrolmentHelper::getTeeToken (
    const ErpArray<TpmObjectNameLength>& akName,
    const Tpm::QuoteOutput& quote,
    const ErpBlob& knownAk,
    const ErpBlob& nonce,
    const ErpBlob& trustedQuote)
{
    auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryEnrolmentHelper,
                                                         "EnrolmentHelper:ERP_GetTEEToken");

    hsmclient::TEETokenRequestInput input;

    writeArray(akName, input.AKName);

    writeVector(Base64::decode(quote.quotedDataBase64), input.QuoteData, input.QuoteDataLength);
    writeVector(Base64::decode(quote.quoteSignatureBase64), input.QuoteSignature, input.QuoteSignatureLength);
    writeBlob(knownAk, input.KnownAKBlob);
    writeBlob(nonce, input.NONCEBlob);
    writeBlob(trustedQuote, input.KnownQuoteBlob);

    auto output = ERP_GetTEEToken(mHsmSession.rawSession, input);

    HsmExpectSuccess(output, "ERP_TEEToken failed", timer);
    TVLOG(logLevel) << "successfully called ERP_TEEToken";

    return convertBlob(output.BlobOut);
}

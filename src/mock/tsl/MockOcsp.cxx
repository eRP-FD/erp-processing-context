/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "MockOcsp.hxx"
#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/tsl/X509Certificate.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/FileHelper.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "erp/util/OpenSsl.hxx"
#include "erp/util/SafeString.hxx"
#include "mock/util/MockConfiguration.hxx"

#include <functional>
#include <memory>


namespace
{
    std::unique_ptr<OCSP_CERTID, decltype(&OCSP_CERTID_free)>
    createUnexpectedCertId()
    {
        const auto pkiPath = MockConfiguration::instance().getPathValue(MockConfigurationKey::MOCK_GENERATED_PKI_PATH);
        const Certificate certificate = Certificate::fromBase64Der(FileHelper::readFileAsString(
            pkiPath / "../tsl/X509Certificate/QES-noType.base64.der"));
        const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
            pkiPath / "../tsl/X509Certificate/QES-noTypeCA.base64.der"));
        return std::unique_ptr<OCSP_CERTID, decltype(&OCSP_CERTID_free)>(
            OCSP_cert_to_id(nullptr, certificate.toX509(), certificateCA.toX509()), OCSP_CERTID_free);
    }

    std::unique_ptr<OCSP_CERTID, decltype(&OCSP_CERTID_free)>
    getCertificateIdFromOcspRequest(const std::string& ocspRequest)
    {
        const unsigned char* buffer = reinterpret_cast<const unsigned char*>(ocspRequest.data());
        std::unique_ptr<OCSP_REQUEST, decltype(&OCSP_REQUEST_free)> request(
            d2i_OCSP_REQUEST(nullptr, &buffer, gsl::narrow_cast<long>(ocspRequest.size())),
            OCSP_REQUEST_free);
        OpenSslExpect(request != nullptr, "can not read ocsp request");

        const int requestCount = OCSP_request_onereq_count(request.get());
        OpenSslExpect(requestCount == 1,
                      "Gematik requires that there is only one Certificate in OCSP request");

        OCSP_ONEREQ* oneRequest = OCSP_request_onereq_get0(request.get(), 0);
        OpenSslExpect(oneRequest != nullptr, "can not retrieve one request from OCSP request");

        OCSP_CERTID* ocspCertId = OCSP_onereq_get0_id(oneRequest);
        OpenSslExpect(ocspCertId != nullptr, "can not get certificate id from OCSP request");

        return std::unique_ptr<OCSP_CERTID, decltype(&OCSP_CERTID_free)>(OCSP_CERTID_dup(ocspCertId), OCSP_CERTID_free);
    }


    std::unique_ptr<ASN1_OCTET_STRING, std::function<void(ASN1_OCTET_STRING*)>>
    sequenceToString(const ASN1_SEQUENCE_ANY& sequence)
    {
        int stringLength = i2d_ASN1_SEQUENCE_ANY(&sequence, nullptr);
        OpenSslExpect(stringLength > 0, "can not get length for extension string");
        std::vector<unsigned char> extensionSequenceData(static_cast<size_t>(stringLength), 0u);
        unsigned char* rawData = extensionSequenceData.data();
        OpenSslExpect(stringLength == i2d_ASN1_SEQUENCE_ANY(&sequence, &rawData),
                      "can not convert sequence to string");

        std::unique_ptr<ASN1_OCTET_STRING, std::function<void(ASN1_OCTET_STRING*)>> sequenceString(
            ASN1_OCTET_STRING_new(), ASN1_OCTET_STRING_free);
        OpenSslExpect(sequenceString != nullptr, "Can not create ASN1_OCTET_STRING");
        OpenSslExpect(1 == ASN1_OCTET_STRING_set(
            sequenceString.get(), extensionSequenceData.data(), gsl::narrow_cast<int>(extensionSequenceData.size())),
                      "can not set ASN1_OCTET_STRING");

        return sequenceString;
    }

    void addHashExtension(const MockOcsp::CertificatePair& certificateToSign,
                          OCSP_SINGLERESP& singleResponse)
    {
        unsigned char messageDigest[EVP_MAX_MD_SIZE];
        unsigned int digestLength = EVP_MAX_MD_SIZE;

        if (certificateToSign.testMode == MockOcsp::CertificateOcspTestMode::CERTHASH_MISMATCH)
        {
            std::fill_n(messageDigest, digestLength, '0');
        }
        else
        {
            OpenSslExpect(1 == X509_digest(certificateToSign.certificate.toX509(), EVP_sha256(), messageDigest, &digestLength),
                          "can not create certificate digest");
        }

        std::unique_ptr<ASN1_OCTET_STRING, std::function<void(ASN1_OCTET_STRING*)>> hash(
            ASN1_OCTET_STRING_new(), ASN1_OCTET_STRING_free);
        OpenSslExpect(hash != nullptr, "Can not create ASN1_OCTET_STRING");
        OpenSslExpect(1 == ASN1_OCTET_STRING_set(hash.get(), messageDigest, gsl::narrow<int>(digestLength)),
                      "can not set ASN1_OCTET_STRING");

        shared_ASN1_TYPE hashType = shared_ASN1_TYPE::make();
        OpenSslExpect(1 == ASN1_TYPE_set1(hashType.get(), V_ASN1_OCTET_STRING, hash.get()),
                      "can not set ASN1_TYPE");

        shared_ASN1_OBJECT digestObject = shared_ASN1_OBJECT::make(OBJ_nid2obj(NID_sha256));
        OpenSslExpect(digestObject != nullptr, "can not create digest algorithm object");

        shared_ASN1_TYPE digestType = shared_ASN1_TYPE::make();
        OpenSslExpect(1 == ASN1_TYPE_set1(digestType.get(), V_ASN1_OBJECT, digestObject),
                      "can not set ASN1_TYPE");

        shared_ASN1_SEQUENCE_ANY digestSequence = shared_ASN1_SEQUENCE_ANY::make();
        OpenSslExpect(1 == sk_ASN1_TYPE_push(digestSequence.get(), digestType.get()),
                      "can not push digest to sequence");
        digestType.release();

        std::unique_ptr<ASN1_OCTET_STRING , std::function<void(ASN1_OCTET_STRING*)>> digestSequenceString =
            sequenceToString(*digestSequence);
        OpenSslExpect(digestSequenceString != nullptr, "can not create digest sequence string");

        shared_ASN1_TYPE digestSequenceType = shared_ASN1_TYPE::make();
        OpenSslExpect(1 == ASN1_TYPE_set1(digestSequenceType.get(), V_ASN1_SEQUENCE, digestSequenceString.get()),
                      "can not set ASN1_TYPE");

        shared_ASN1_SEQUENCE_ANY extensionSequence = shared_ASN1_SEQUENCE_ANY::make();
        OpenSslExpect(1 == sk_ASN1_TYPE_push(extensionSequence.get(), digestSequenceType.get()),
                      "can not add data to sequence");
        digestSequenceType.release();
        OpenSslExpect(2 == sk_ASN1_TYPE_push(extensionSequence.get(), hashType.get()),
                      "can not add data to sequence");
        hashType.release();

        std::unique_ptr<ASN1_OCTET_STRING , std::function<void(ASN1_OCTET_STRING*)>> extensionString =
            sequenceToString(*extensionSequence);
        OpenSslExpect(digestSequenceString != nullptr, "can not create digest sequence string");

        const shared_ASN1_OBJECT extensionIdObject = shared_ASN1_OBJECT::make(OBJ_txt2obj("1.3.36.8.3.13", 1));
        OpenSslExpect(extensionIdObject != nullptr, "can not create object with extension id");

        std::unique_ptr<X509_EXTENSION , std::function<void(X509_EXTENSION*)>> extension(
            X509_EXTENSION_create_by_OBJ(nullptr, extensionIdObject.get(), 0, extensionString.get()),
            X509_EXTENSION_free);
        OpenSslExpect(extension != nullptr, "can not create OCSP extension");

        OpenSslExpect(1 == OCSP_SINGLERESP_add_ext(&singleResponse, extension.get(), -1),
                      "can not add extension to basic OCSP response");
    }


    std::optional<MockOcsp::CertificatePair> findCertificateById(
        const std::vector<MockOcsp::CertificatePair>& ocspResponderKnownCertificateCaPairs,
        OCSP_CERTID& certificateId)
    {
        ASN1_OBJECT* certificateIdMdOid = nullptr;
        OpenSslExpect(1 == OCSP_id_get0_info(nullptr, &certificateIdMdOid, nullptr, nullptr, &certificateId),
                      "can not get info from certificate id");
        OpenSslExpect(certificateIdMdOid != nullptr, "can not get MD OID");

        const EVP_MD* certificateIdMd = EVP_get_digestbyobj(certificateIdMdOid);
        OpenSslExpect(certificateIdMd != nullptr, "can not get EVP_MD object from OID");

        for(const auto& certificatePair : ocspResponderKnownCertificateCaPairs)
        {
            std::unique_ptr<OCSP_CERTID, decltype(&OCSP_CERTID_free)> knownCertificateId(
                OCSP_cert_to_id(nullptr, certificatePair.certificate.toX509(), certificatePair.issuer.toX509()),
                OCSP_CERTID_free);
            OpenSslExpect(knownCertificateId != nullptr, "can not create certificate id");
            if (0 == OCSP_id_cmp(&certificateId, knownCertificateId.get()))
            {
                return certificatePair;
            }
        }

        return std::nullopt;
    }
}


MockOcsp MockOcsp::create (
    const std::string& ocspRequest,
    const std::vector<CertificatePair>& ocspResponderKnownCertificateCaPairs,
    Certificate& certificate,
    shared_EVP_PKEY& privateKey)
{
    shared_OCSP_BASICRESP basicResponse =  shared_OCSP_BASICRESP::make();

    std::vector<unsigned char> keybytes(128, 48u);
    shared_ASN1_BIT_STRING key = shared_ASN1_BIT_STRING::make();
    OpenSslExpect(ASN1_BIT_STRING_set(key.get(), keybytes.data(), gsl::narrow_cast<int>(keybytes.size())), "can not set key");

    std::unique_ptr<OCSP_CERTID, decltype(&OCSP_CERTID_free)> certificateId =
        getCertificateIdFromOcspRequest(ocspRequest);
    OpenSslExpect(certificateId != nullptr, "can not create certificate id");

    const std::optional<MockOcsp::CertificatePair> certificateToSign =
        findCertificateById(ocspResponderKnownCertificateCaPairs, *certificateId);
    CertificateOcspTestMode testMode =
        certificateToSign.has_value() ? certificateToSign->testMode : CertificateOcspTestMode::SUCCESS;

    if (testMode == CertificateOcspTestMode::WRONG_CERTID)
    {
        certificateId = createUnexpectedCertId();
        OpenSslExpect(certificateId != nullptr, "can not create unexpected certificate id");
    }

    const auto thisUpdateTime = (testMode == CertificateOcspTestMode::WRONG_THIS_UPDATE
                             ? time(nullptr) + 6000
                             : time(nullptr));
    std::unique_ptr<ASN1_TIME, decltype(&ASN1_TIME_free)> thisUpdate(
        ASN1_TIME_set(nullptr, thisUpdateTime), ASN1_TIME_free);
    OpenSslExpect(thisUpdate != nullptr, "can not create timestamp of this update");

    std::unique_ptr<ASN1_TIME, decltype(&ASN1_TIME_free)> nextUpdate(
        ASN1_TIME_set(nullptr, thisUpdateTime + 60), ASN1_TIME_free);
    OpenSslExpect(nextUpdate != nullptr, "can not create timestamp of next update");

    int status = V_OCSP_CERTSTATUS_UNKNOWN;
    if (certificateToSign.has_value())
    {
        status = (testMode == CertificateOcspTestMode::REVOKED
                      ? V_OCSP_CERTSTATUS_REVOKED
                      : V_OCSP_CERTSTATUS_GOOD);
    }

    OCSP_SINGLERESP* singleResponse =
        OCSP_basic_add1_status(basicResponse.get(),
            certificateId.get(),
            status,
            0,
            status == V_OCSP_CERTSTATUS_REVOKED ? thisUpdate.get() : nullptr,
            thisUpdate.get(),
            nextUpdate.get());
    OpenSslExpect(singleResponse != nullptr, "can not set status in basic ocsp response");

    if (certificateToSign.has_value() && testMode != CertificateOcspTestMode::CERTHASH_MISSING)
    {
        addHashExtension(*certificateToSign, *singleResponse);
    }

    unsigned char nonce[5] = "test";
    OpenSslExpect( 1 == OCSP_basic_add1_nonce(basicResponse.get(), nonce, sizeof nonce),
                  "can not add ocsp nonce");

    OpenSslExpect(
        1 == OCSP_basic_sign(
                 basicResponse.get(),
                 certificate.toX509(),
                 privateKey.get(),
                 EVP_sha1(),
                 nullptr,
                 (testMode == CertificateOcspTestMode::WRONG_PRODUCED_AT ? OCSP_NOTIME : 0)),
        "can not sign basic ocsp response");

    auto response = shared_OCSP_RESPONSE::make(OCSP_response_create(V_OCSP_CERTSTATUS_GOOD, basicResponse.get()));
    OpenSslExpect(response != nullptr, "can not create OCSP response");

    return MockOcsp(std::move(response));
}


MockOcsp MockOcsp::fromPemString(const std::string& pem)
{
    std::unique_ptr<BIO, int(*)(BIO*)> mem (BIO_new(BIO_s_mem()), BIO_free);
    Expect(mem != nullptr, "can not allocate memory");

    BIO_write(mem.get(),pem.data(), gsl::narrow_cast<int>(pem.size()));
    // Note: this is basically the definition of PEM_read_bio_OCSP_RESPONSE from ocsp.h
    // but with a cast of the function (first argument) that compiles. Not sure why
    // PEM_read_bio_OCSP_RESPONSE uses a wrong cast. Maybe it was written for pure C compilers
    // or for less strict warning levels.
    auto ocsp = shared_OCSP_RESPONSE::make(
        reinterpret_cast<OCSP_RESPONSE*>(
            PEM_ASN1_read_bio(
                reinterpret_cast<void*(*)(void**,const unsigned char**,long int)>(d2i_OCSP_RESPONSE),
                PEM_STRING_OCSP_RESPONSE,
                mem.get(),
                nullptr,
                nullptr,
                nullptr)));

    return MockOcsp(std::move(ocsp));
}


MockOcsp::MockOcsp (shared_OCSP_RESPONSE ocspResponse)
    : mOcspResponse(std::move(ocspResponse))
{
}

std::string MockOcsp::toDer(void)
{
    std::unique_ptr<BIO, int(*)(BIO*)> mem (BIO_new(BIO_s_mem()), BIO_free);
    Expect(mem != nullptr, "can not allocate memory");

    // See comment regarding cast in ::toPemString.
    ASN1_i2d_bio(
        reinterpret_cast<int (*) (void*, unsigned char**)>(i2d_OCSP_RESPONSE),
        mem.get(),
        reinterpret_cast<unsigned char *>(mOcspResponse.get()));
    return bioToString(mem.get());
}

std::string MockOcsp::toPemString (void)
{
    std::unique_ptr<BIO, int(*)(BIO*)> mem (BIO_new(BIO_s_mem()), BIO_free);
    Expect(mem != nullptr, "can not allocate memory");

    // Note: this is basically the definition of PEM_read_bio_OCSP_RESPONSE from ocsp.h
    // but with a cast of the function (first argument) that compiles. Not sure why
    // PEM_read_bio_OCSP_RESPONSE uses a wrong cast. Maybe it was written for pure C compilers
    // or for less strict warning levels.
    PEM_ASN1_write_bio(
        reinterpret_cast<int(*)(void*,unsigned char**)>(i2d_OCSP_RESPONSE),
        PEM_STRING_OCSP_RESPONSE,
        mem.get(),
        reinterpret_cast<char *>(mOcspResponse.get()),
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr);

    return bioToString(mem.get());
}


bool MockOcsp::verify (const std::chrono::duration<int>) const
{
    // This being a mock OCSP response, we always return true.
    return true;
}


bool MockOcsp::verify (const Certificate&) const
{
    // This being a mock OCSP response, we always return true.
    return true;
}

/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/crypto/Certificate.hxx"

#include "erp/util/Base64.hxx"
#include "erp/util/String.hxx"


#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string_view>


namespace
{
    constexpr std::string_view PEM_CERTIFICATE_BEGIN_TAG = "-----BEGIN CERTIFICATE-----";

    using Asn1PrintableStringPtr = const std::unique_ptr<
                                                        ASN1_PRINTABLESTRING,
                                                        std::function<void(ASN1_PRINTABLESTRING*)>>;

    using ProfessionInfoPtr = const std::unique_ptr<PROFESSION_INFO,
                                                    std::function<void(PROFESSION_INFO*)>>;

    using ProfessionInfoListPtr = const std::unique_ptr<PROFESSION_INFOS,
                                                        std::function<void(PROFESSION_INFOS*)>>;

    using AdmissionPtr = const std::unique_ptr<ADMISSIONS,
                                               std::function<void(ADMISSIONS*)>>;

    using AdmissionListPtr = const std::unique_ptr<STACK_OF(ADMISSIONS),
                                                   std::function<void(STACK_OF(ADMISSIONS)*)>>;

    using AdmissionSyntaxPtr = const std::unique_ptr<ADMISSION_SYNTAX,
                                                     std::function<void(ADMISSION_SYNTAX*)>>;

    std::function<void(ASN1_PRINTABLESTRING*)> createAsn1PrintableStringDeleter (bool& cleanupRequired)
    {
        return [&cleanupRequired](auto* asn1RegistrationNumberRawPtr)
        {
            if (cleanupRequired)
            {
                // technically not needed here as this
                // is the last unique_ptr to be destroyed
                // and therefore after this step,
                // `cleanupRequired` becomes irrelevant
                //
                if (ASN1_STRING_get0_data(
                    asn1RegistrationNumberRawPtr))
                {
                    cleanupRequired = false;
                }

                ASN1_PRINTABLESTRING_free(
                    asn1RegistrationNumberRawPtr);
            }
        };
    }


    std::function<void(PROFESSION_INFO*)> createProfessionInfoDeleter (bool& cleanupRequired)
    {
        return [&cleanupRequired](auto* professionInfoRawPtr)
        {
            if (cleanupRequired)
            {
                PROFESSION_INFO_free(professionInfoRawPtr);
            }
        };
    }


    std::function<void(PROFESSION_INFOS*)> createProfessionInfoListDeleter (bool& cleanupRequired)
    {
        return [&cleanupRequired](auto* professionInfoListRawPtr)
        {
            if (cleanupRequired)
            {
                if (sk_PROFESSION_INFO_num(
                    professionInfoListRawPtr) > 0)
                {
                    cleanupRequired = false;
                }

                sk_PROFESSION_INFO_pop_free(
                    professionInfoListRawPtr,
                    PROFESSION_INFO_free);
            }
        };
    }


    std::function<void(ADMISSIONS*)> createAdmissionDeleter (bool& cleanupRequired)
    {
        return [&cleanupRequired](auto* admissionRawPtr)
        {
            if (cleanupRequired)
            {
                if (ADMISSIONS_get0_professionInfos(admissionRawPtr))
                {
                    cleanupRequired = false;
                }

                ADMISSIONS_free(admissionRawPtr);
            }
        };
    }

    std::function<void(STACK_OF(ADMISSIONS)*)> createAdmissionListDeleter (bool& cleanupRequired)
    {
        return [&cleanupRequired](auto* admissionListRawPtr)
        {
            if (cleanupRequired)
            {
                if (sk_ADMISSIONS_num(admissionListRawPtr) > 0)
                {
                    cleanupRequired = false;
                }

                sk_ADMISSIONS_pop_free(admissionListRawPtr,
                    ADMISSIONS_free);
            }
        };
    }


    std::function<void(ADMISSION_SYNTAX*)> createAdmissionSyntaxDeleter (bool& cleanupRequired)
    {
        return [&cleanupRequired](auto* admissionSyntaxRawPtr)
        {
            if (cleanupRequired)
            {
                if (ADMISSION_SYNTAX_get0_contentsOfAdmissions(
                    admissionSyntaxRawPtr))
                {
                    cleanupRequired = false;
                }

                ADMISSION_SYNTAX_free(admissionSyntaxRawPtr);
            }
        };
    }



    void extendName (X509_NAME* name, const std::vector<std::string>& parts)
    {
        if ((parts.size() % 2) != 0)
            throw std::logic_error("expecting key/value pairs for name construction");

        for (std::size_t index=0; index<parts.size(); index+=2)
        {
            X509_NAME_add_entry_by_txt(
                name,
                parts[index].c_str(),
                MBSTRING_ASC,
                reinterpret_cast<const unsigned char*>(parts[index + 1].c_str()),
                -1,
                -1,
                0);
        }
    }

    void throwIfNot (const bool expectedTrue, const std::string& message, const std::string& location)
    {
        if ( ! expectedTrue)
        {
            showAllOpenSslErrors();
            throw std::runtime_error("certificate error at " + location +"\n    " + message);
        }
    }

    #define throwIfNot(expression, message) \
        throwIfNot((expression), message, std::string(__FILE__) + ":" + std::to_string(__LINE__))
}


Certificate::Certificate (shared_X509 x509Certificate)
    : mX509Certificate(std::move(x509Certificate))
{
}


Certificate Certificate::fromBinaryDer(const std::string& binaryDer)
{
    const auto* derData = reinterpret_cast<const unsigned char*>(binaryDer.data());
    auto certificate = shared_X509::make(
        d2i_X509(nullptr, &derData, static_cast<int>(binaryDer.size())));
    throwIfNot(
        certificate.isSet(),
        "invalid DER certificate string");

    return Certificate(std::move(certificate));
}


Certificate Certificate::fromBase64(const std::string& base64)
{
    if (String::contains(base64, PEM_CERTIFICATE_BEGIN_TAG))
    {
        return Certificate::fromPem(base64);
    }

    return Certificate::fromBase64Der(base64);
}


Certificate Certificate::fromPem (const std::string& pem)
{
    auto mem = shared_BIO::make();
    const auto trimmedPem = String::trim(pem);
    const int written = BIO_write(mem, trimmedPem.data(), static_cast<int>(trimmedPem.size()));
    throwIfNot(written>=0 && static_cast<size_t>(written) == trimmedPem.size(),
               "attempt to write bytes has failed");

    auto certificate = shared_X509::make(PEM_read_bio_X509(mem, nullptr, nullptr, nullptr));
    throwIfNot(
        certificate.isSet(),
        "can not convert PEM string to certificate");

    return Certificate(std::move(certificate));
}


Certificate Certificate::fromBase64Der (const std::string& base64Der)
{
    const std::string derString = Base64::decodeToString(Base64::cleanupForDecoding(base64Der));

    auto mem = shared_BIO::make();
    const int written = BIO_write(mem, derString.data(), static_cast<int>(derString.size()));
    throwIfNot(
        static_cast<int>(derString.size()) == written,
        "can not fill internal buffer");

    return fromBinaryDer(derString);
}


std::string Certificate::toPem(void) const
{
    auto certificateMemory = shared_BIO::make();
    const int status = PEM_write_bio_X509(certificateMemory,
                                          const_cast<X509*>(mX509Certificate.get()));
    throwIfNot(
        status == 1,
        "can not convert certificate to PEM string");

    char* data = nullptr;
    const std::size_t length = BIO_get_mem_data(certificateMemory, &data);
    throwIfNot(data != nullptr && length != 0, "can not get data from certificate");

    std::string s (data, length);

    return s;
}


std::string Certificate::toBase64Der(void) const
{
    return Base64::encode(toBinaryDer());
}


std::string Certificate::toBinaryDer(void) const
{
    auto certificateMemory = shared_BIO::make();
    const int status = i2d_X509_bio(certificateMemory, mX509Certificate.removeConst());
    throwIfNot(
        status == 1,
        "can not convert certificate to DER string");

    char* data = nullptr;
    const std::size_t length = BIO_get_mem_data(certificateMemory, &data);
    throwIfNot(data != nullptr && length != 0, "can not get data from certificate");

    return {data, length};
}


Certificate::Certificate (const Certificate& other) // NOLINT(misc-no-recursion)
    : mX509Certificate(other.mX509Certificate),
      mNextCertificate()
{
    if (other.mNextCertificate)
        mNextCertificate = std::make_unique<Certificate>(*other.mNextCertificate);
}


shared_EVP_PKEY Certificate::getPublicKey (void) const
{
    auto publicKeyRootCertificate = shared_EVP_PKEY::make(
        X509_get_pubkey(mX509Certificate.removeConst()));
    if ( ! publicKeyRootCertificate.isSet())
        throw std::runtime_error("can not extract public key from certificate");
    return publicKeyRootCertificate;
}


const X509_NAME* Certificate::getSubjectName (void) const
{
    return X509_get_subject_name(mX509Certificate);
}


const X509_NAME* Certificate::getIssuerName (void) const
{
    return X509_get_issuer_name(mX509Certificate);
}


bool Certificate::isEllipticCurve (void) const
{
    auto pkey = getPublicKey();
    return pkey && EVP_PKEY_base_id(pkey.get()) == EVP_PKEY_EC;
}


shared_X509 Certificate::toX509 (void)
{
    return mX509Certificate;
}

const shared_X509& Certificate::toX509() const
{
    return mX509Certificate;
}

void Certificate::setNextCertificate (const Certificate& certificate)
{
    mNextCertificate = std::make_unique<Certificate>(certificate.mX509Certificate);
}


const Certificate* Certificate::getNextCertificate (void) const
{
    return mNextCertificate.get();
}


Certificate::Builder Certificate::build (void)
{
    return {};
}



Certificate::Builder::Builder (void)
    : mX509Certificate(shared_X509::make())
{
}


Certificate::Builder& Certificate::Builder::withRandomSerial (void)
{
    auto bignum = shared_BN::make();
    const int status1 = BN_pseudo_rand(bignum, 64, 0, 0);
    if (status1 == 0)
        throw std::runtime_error("can not create pseudo random value for random serial");

    auto ai = shared_ASN1_INTEGER::make(
        BN_to_ASN1_INTEGER(bignum, nullptr));
    if ( ! ai.isSet())
        throw std::runtime_error("can not convert BIGNUM into ASN1 integer");

    X509_set_serialNumber(mX509Certificate, ai);

    return *this;
}


Certificate::Builder& Certificate::Builder::withSerial (const long int serial)
{
    asn1_string_st* serialNumber = X509_get_serialNumber(mX509Certificate);
    throwIfNot(serialNumber != nullptr, "can not get serial number");

    ASN1_INTEGER_set(serialNumber, serial);
    return *this;
}


Certificate::Builder& Certificate::Builder::withSubjectName (const X509_NAME* name)
{
    X509_set_subject_name(mX509Certificate, const_cast<X509_NAME*>(name));
    return *this;
}


Certificate::Builder& Certificate::Builder::withSubjectName (const std::vector<std::string>& parts)
{
    auto* name = X509_get_subject_name(mX509Certificate);
    throwIfNot(name != nullptr, "can not get name");

    extendName(name, parts);
    return withSubjectName(name);
}


Certificate::Builder& Certificate::Builder::withIssuerName (const X509_NAME* name)
{
    X509_set_issuer_name(mX509Certificate, const_cast<X509_NAME*>(name));
    return *this;
}


Certificate::Builder& Certificate::Builder::withIssuerName (const std::vector<std::string>& parts)
{
    auto* name = X509_get_issuer_name(mX509Certificate);
    throwIfNot(name != nullptr, "can not get name");

    extendName(name, parts);
    return withIssuerName(name);
}


Certificate::Builder& Certificate::Builder::fromNow (void)
{
    asn1_string_st* notBefore = X509_get_notBefore(mX509Certificate);
    throwIfNot(notBefore != nullptr, "can not get notBefore info");

    X509_gmtime_adj(notBefore, 0);
    return *this;
}


Certificate::Builder& Certificate::Builder::until (const std::chrono::duration<int>& end)
{
    asn1_string_st* notAfter = X509_get_notAfter(mX509Certificate);
    throwIfNot(notAfter != nullptr, "can not get notAfter info");

    auto endInSecs = std::chrono::duration_cast<std::chrono::seconds>(end).count();
    throwIfNot(endInSecs <= std::numeric_limits<long>::max(), "end time out of range");

    X509_gmtime_adj(notAfter, static_cast<long>(endInSecs));
    return *this;
}


Certificate::Builder& Certificate::Builder::withPublicKey (const shared_EVP_PKEY& publicKey)
{
    ErpExpect(publicKey.isSet(), HttpStatus::InternalServerError, "public key is not set");
    X509_set_pubkey(mX509Certificate, publicKey.removeConst());
    return *this;
}




Certificate::Builder& Certificate::Builder::withRegistrationNumber (
    const std::string& registrationNumber)
{
    // BEWARE: memory management in this method is tricky, to say the least.
    //
    // Because of a combination of poor API design and ownership semantics in OpenSSL,
    // *and* bulletproof robustness of application code, which was definitely not absolutely
    // required at this time and location and thus can be regarded as rather an over-engineering,
    // the mechanisms by which memory is freed and when that happens exactly is very cumbersome.
    //
    // As a short summary, the declaration order of the various unique_ptrs is of utter importance.
    // Because later unique_ptrs get to own earlier ones through OpenSSL inner workings,
    // at each step of the (reverse) destruction order we need to question whether manual
    // clean up is still required. If so, then perform it and then evaluate if the condition
    // still holds for the next to-be-destroyed unique_ptr. This evaluation depends on whether
    // the above-mentioned OpenSSL inner workings have been applied or not (due to an exception).
    //
    // Through some template magic, quite a lot of the at-first-sight-duplication can be
    // gotten rid of in the below unique_ptr declarations; but since the complexity level
    // is already huge for the simple task of building some nested data structures and
    // serializing them into ASN1.DER, the clever template code has been omitted altogether.
    //
    bool cleanupRequired = true;

    Asn1PrintableStringPtr asn1RegistrationNumber{ASN1_PRINTABLESTRING_new(),
                                                  createAsn1PrintableStringDeleter(cleanupRequired)};
    if (1 != ASN1_STRING_set(asn1RegistrationNumber.get(),
                             registrationNumber.c_str(),
                             static_cast<int>(registrationNumber.size())))
    {
        throw std::runtime_error{"cannot set the registrationNumber into a ASN1_PRINTABLESTRING"};
    }

    ProfessionInfoPtr professionInfo{PROFESSION_INFO_new(),
                                     createProfessionInfoDeleter(cleanupRequired)};

    PROFESSION_INFO_set0_registrationNumber(professionInfo.get(), asn1RegistrationNumber.get());

    ProfessionInfoListPtr professionInfoList{sk_PROFESSION_INFO_new_reserve(nullptr, 1),
                                             createProfessionInfoListDeleter(cleanupRequired)};

    sk_PROFESSION_INFO_push(professionInfoList.get(), professionInfo.get());

    AdmissionPtr admission{ADMISSIONS_new(),
                           createAdmissionDeleter(cleanupRequired)};

    ADMISSIONS_set0_professionInfos(admission.get(), professionInfoList.get());

    AdmissionListPtr admissionList{sk_ADMISSIONS_new_reserve(nullptr, 1),
                                   createAdmissionListDeleter(cleanupRequired)};

    sk_ADMISSIONS_push(admissionList.get(), admission.get());

    AdmissionSyntaxPtr admissionSyntax{ADMISSION_SYNTAX_new(),
                                       createAdmissionSyntaxDeleter(cleanupRequired)};

    ADMISSION_SYNTAX_set0_contentsOfAdmissions(admissionSyntax.get(), admissionList.get());

    const auto admissionExtensionNid{OBJ_ln2nid("Professional Information or basis for Admission")};
    if (NID_undef == admissionExtensionNid)
    {
        throw std::runtime_error{"cannot find the NID for the admission syntax x509v3 extension "
                                 "[TeleTrust, OID 1.3.36.8.3.3]"};
    }

    if (1 != X509_add1_ext_i2d(mX509Certificate,
                               admissionExtensionNid,
                               admissionSyntax.get(),
                               0,
                               0))
    {
        throw std::runtime_error{"cannot add admission syntax extension to X509 certificate"};
    }

    return *this;
}


Certificate Certificate::Builder::build (void) const
{
    return Certificate(mX509Certificate);
}


Certificate Certificate::createSelfSignedCertificateMock (
    const shared_EVP_PKEY& keyPair,
    const AuthorizedIdentity& authorizedIdentity,
    const std::string& hostname,
    const std::optional<std::string>& registrationNumber)
{
    std::vector<std::string> subject = {
        "C", "DE",
        "O", "mock-certificate-authority",
        "CN", hostname};

    if (authorizedIdentity.type == AuthorizedIdentity::IDENTITY_SUBJECT_ID)
    {
        subject.emplace_back("OU");
        subject.emplace_back(authorizedIdentity.identity);
    }

    auto builder = Certificate::build()
        .withSerial(1234567890L) // Use a non-trivial but static serial number.
        .withSubjectName(subject)
        .fromNow()
        .until(std::chrono::minutes(60))
        .withIssuerName({
            "C", "DE",
            "O", "mock-certificate-authority",
            "CN", hostname})
        .withSubjectName({
            "C", "DE",
            "O", "mock-certificate-authority",
            "CN", hostname})
        .withPublicKey(keyPair);

    if (registrationNumber)
    {
        builder.withRegistrationNumber(*registrationNumber);
    }

    auto certificate = builder.build();

    const int signatureLength = X509_sign(certificate.toX509(),
                                          keyPair.removeConst(),
                                          EVP_sha256());
    throwIfNot(
        signatureLength > 0,
        "creating signature for X509 certificate failed");

    return certificate;
}


Certificate Certificate::createCertificateMock (
    const Certificate& rootCertificate,
    const shared_EVP_PKEY& rootCertificatePrivateKey,
    const shared_EVP_PKEY& keyPair)
{
    auto certificate = Certificate::build()
        .withSerial(2012345678L) // Use a non-trivial but static serial number.
        .fromNow()
        .until(std::chrono::minutes(60))
        .withIssuerName(rootCertificate.getSubjectName())
        .withSubjectName({
            "C", "DE",
            "O", "mock-organization",
            "CN", "localhost"})
        .withPublicKey(keyPair)
        .build();
    showAllOpenSslErrors();

    auto rootCertificatePublicKey = rootCertificatePrivateKey;
    const int status = EVP_PKEY_copy_parameters(rootCertificatePublicKey, rootCertificatePrivateKey);
    throwIfNot(
        status == 1,
        "can not copy root key parameters");

    const int signatureLength = X509_sign(certificate.toX509(),
                                          rootCertificatePrivateKey.removeConst(),
                                          EVP_sha256());
    throwIfNot(
        signatureLength > 0,
        "can not sign new certificate");

    certificate.setNextCertificate(rootCertificate);

    return certificate;
}

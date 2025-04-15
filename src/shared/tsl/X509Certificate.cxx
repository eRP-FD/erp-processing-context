/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/tsl/X509Certificate.hxx"

#include <regex>
#include <stdexcept>
#include <unordered_set>

#include "shared/crypto/EllipticCurvePublicKey.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/crypto/Certificate.hxx"
#include "shared/tsl/error/TslError.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Expect.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "shared/crypto/OpenSsl.hxx"


class QcStatement {
public:
    ASN1_OBJECT *statementId;
    ASN1_TYPE *statementInfo;
};
DECLARE_ASN1_FUNCTIONS(QcStatement)
ASN1_SEQUENCE(QcStatement) = {
    ASN1_SIMPLE(QcStatement, statementId, ASN1_OBJECT),
    ASN1_OPT(QcStatement, statementInfo, ASN1_ANY)
} ASN1_SEQUENCE_END(QcStatement)
IMPLEMENT_ASN1_FUNCTIONS(QcStatement)

using QcStatements = STACK_OF(QcStatement);
DEFINE_STACK_OF(QcStatement)
DECLARE_ASN1_FUNCTIONS(QcStatements)
ASN1_ITEM_TEMPLATE(QcStatements) =
    ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, statements, QcStatement)
ASN1_ITEM_TEMPLATE_END(QcStatements)
IMPLEMENT_ASN1_FUNCTIONS(QcStatements)

namespace
{
    using AdmissionSyntaxPtr = const std::unique_ptr<ADMISSION_SYNTAX,
                                                     decltype(&ADMISSION_SYNTAX_free)>;

    const std::string teletrustAdmissionSyntaxOid = "1.3.36.8.3.3";
    const std::regex notEmptyRegex{".+"};

    std::string asn1StringToUtf8(const ASN1_IA5STRING* string)
    {
        unsigned char* utf8Bytes = nullptr;
        auto size = ASN1_STRING_to_UTF8(&utf8Bytes, string);
        std::unique_ptr<unsigned char, void (*)(unsigned char*)> utf8BytesDeleter{
            utf8Bytes, [](auto* data) { OPENSSL_free(data); }};
        if (size < 0)
            return std::string{};
        return std::string{
            reinterpret_cast<char*>(utf8Bytes), gsl::narrow<std::string::size_type>(size)};
    }


    bool checkProfessionInfo(const ProfessionInfo_st* professionInfo,
                             const std::string& telematikId)
    {
        if (professionInfo)
        {
            const auto* registrationNumberAsn1 =
                PROFESSION_INFO_get0_registrationNumber(professionInfo);
            if (registrationNumberAsn1)
            {
                const auto* registrationNumber = ASN1_STRING_get0_data(registrationNumberAsn1);
                if (registrationNumber &&
                    telematikId ==
                    std::string{reinterpret_cast<const char*>(registrationNumber),
                                gsl::narrow<std::string::size_type>(
                                    ASN1_STRING_length(registrationNumberAsn1))})
                {
                    return true;
                }
            }
        }

        return false;
    }


    bool checkProfessionInfoList(const stack_st_PROFESSION_INFO* professionInfoList,
                                 const std::string& telematikId)
    {
        if (professionInfoList)
        {
            for (auto professionInfoItr = 0;
                 professionInfoItr < sk_PROFESSION_INFO_num(professionInfoList);
                 ++professionInfoItr)
            {
                const auto* professionInfo =
                    sk_PROFESSION_INFO_value(professionInfoList, professionInfoItr);
                if (checkProfessionInfo(professionInfo, telematikId))
                {
                    return true;
                }
            }
        }

        return false;
    }


    bool checkAdmissions(const asn1_string_st* extensionDataAsn1,
                         const unsigned char* extensionData,
                         const std::string& telematikId)
    {

        AdmissionSyntaxPtr admissionSyntax{d2i_ADMISSION_SYNTAX(nullptr,
                                                                &extensionData,
                                                                ASN1_STRING_length(
                                                                    extensionDataAsn1)),
                                           ADMISSION_SYNTAX_free};

        if (admissionSyntax)
        {
            const auto* admissionList =
                ADMISSION_SYNTAX_get0_contentsOfAdmissions(admissionSyntax.get());
            if (admissionList)
            {
                for (auto admissionItr = 0; admissionItr < sk_ADMISSIONS_num(admissionList);
                     ++admissionItr)
                {
                    const auto* admission = sk_ADMISSIONS_value(admissionList, admissionItr);
                    if (admission)
                    {
                        const auto* professionInfoList = ADMISSIONS_get0_professionInfos(admission);
                        if (checkProfessionInfoList(professionInfoList, telematikId))
                        {
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }


    bool checkExtension(X509_extension_st* extension,
                        const std::string& telematikId)
    {
        const auto* extensionDataAsn1 = X509_EXTENSION_get_data(extension);
        if (extensionDataAsn1)
        {
            const auto* extensionData = ASN1_STRING_get0_data(extensionDataAsn1);
            if (extensionData)
            {
                return checkAdmissions(extensionDataAsn1, extensionData, telematikId);
            }
        }

        return false;
    }


    std::optional<std::string> extractTelematikIdFromProfessionInfo(
        const ProfessionInfo_st* professionInfo)
    {
        if (professionInfo)
        {
            const auto* registrationNumberAsn1 =
                PROFESSION_INFO_get0_registrationNumber(professionInfo);
            if (registrationNumberAsn1)
            {
                // registrationNumber is a `PrintableString` according to
                // Tab_PKI_226-01 in gemSpec_PKI
                const auto* registrationNumber = ASN1_STRING_get0_data(registrationNumberAsn1);
                if (registrationNumber)
                {
                    return std::string{
                        reinterpret_cast<const char*>(registrationNumber),
                        gsl::narrow<std::string::size_type>(
                            ASN1_STRING_length(registrationNumberAsn1))};
                }
            }
        }

        return std::nullopt;
    }


    std::optional<std::string> extractTelematikIdFromProfessionInfoList(
        const stack_st_PROFESSION_INFO* professionInfoList)
    {
        if (professionInfoList)
        {
            for (auto professionInfoItr = 0;
                 professionInfoItr < sk_PROFESSION_INFO_num(professionInfoList);
                 ++professionInfoItr)
            {
                const auto* professionInfo =
                    sk_PROFESSION_INFO_value(professionInfoList, professionInfoItr);
                std::optional<std::string> telematikId =
                    extractTelematikIdFromProfessionInfo(professionInfo);
                if (telematikId.has_value())
                {
                    return telematikId;
                }
            }
        }

        return std::nullopt;
    }


    std::optional<std::string> extractTelematikIdFromAdmissions(
        const asn1_string_st* extensionDataAsn1, const unsigned char* extensionData)
    {
        AdmissionSyntaxPtr admissionSyntax{
            d2i_ADMISSION_SYNTAX(nullptr, &extensionData, ASN1_STRING_length(extensionDataAsn1)),
            &ADMISSION_SYNTAX_free};

        if (! admissionSyntax)
        {
            return std::nullopt;
        }
        const auto* admissionList =
            ADMISSION_SYNTAX_get0_contentsOfAdmissions(admissionSyntax.get());
        for (auto admissionItr = 0; admissionItr < sk_ADMISSIONS_num(admissionList); ++admissionItr)
        {
            const auto* admission = sk_ADMISSIONS_value(admissionList, admissionItr);
            if (admission)
            {
                const auto* professionInfoList = ADMISSIONS_get0_professionInfos(admission);
                std::optional<std::string> telematikId =
                    extractTelematikIdFromProfessionInfoList(professionInfoList);
                if (telematikId.has_value())
                {
                    return telematikId;
                }
            }
        }

        return std::nullopt;
    }


    std::optional<std::string> extractTelematikIdFromExtension(X509_extension_st * extension)
    {
        const auto* extensionDataAsn1 = X509_EXTENSION_get_data(extension);
        if (extensionDataAsn1)
        {
            const auto* extensionData = ASN1_STRING_get0_data(extensionDataAsn1);
            if (extensionData)
            {
                return extractTelematikIdFromAdmissions(extensionDataAsn1, extensionData);
            }
        }

        return std::nullopt;
    }


    std::optional<std::string> getQcStatement(ASN1_STRING& extensionData, const size_t maxSize)
    {
        std::unique_ptr<QcStatements, decltype(&QcStatements_free)> qcStatements(
            static_cast<QcStatements*>(ASN1_item_unpack(&extensionData, ASN1_ITEM_rptr(QcStatements))),
            QcStatements_free);
        if (qcStatements != nullptr)
        {
            const int size = sk_QcStatement_num(qcStatements.get());
            for (int ind = 0; ind < size; ind++)
            {
                QcStatement* qcStatement = sk_QcStatement_value(qcStatements.get(), ind);
                if (qcStatement != nullptr && qcStatement->statementId)
                {
                    std::string statementIdString(maxSize, 0);
                    int readSize = i2t_ASN1_OBJECT(
                        statementIdString.data(),
                        gsl::narrow_cast<int>(statementIdString.size()),
                        qcStatement->statementId);
                    if (readSize > 0)
                    {
                        statementIdString.resize(static_cast<size_t>(readSize));
                        // only id-qcs-pkixQCSyntax-v1 is supported, means the statementIdString is the oid
                        return statementIdString;
                    }
                }
            }
        }

        return std::nullopt;
    }


    std::optional<std::string> getQcStatement(X509& cert, const size_t maxSize)
    {
        int qcStatementsExtensionInd = X509_get_ext_by_NID(&cert, NID_qcStatements, -1);
        if (qcStatementsExtensionInd >= 0)
        {
            X509_EXTENSION* qcStatementsExtension = X509_get_ext(&cert, qcStatementsExtensionInd);
            if (qcStatementsExtension != nullptr)
            {
                ASN1_STRING* extensionData = X509_EXTENSION_get_data(qcStatementsExtension);
                if (extensionData != nullptr)
                {
                    return getQcStatement(*extensionData, maxSize);
                }
            }
        }

        return std::nullopt;
    }


    bool validateExtensionKnown(X509_EXTENSION* extension, const std::unordered_set<int>& acceptedExtensionNids)
    {
        bool known = false;

        if (extension != nullptr)
        {
            ASN1_OBJECT* extensionObject = X509_EXTENSION_get_object(extension);
            const int exceptionNid = OBJ_obj2nid(extensionObject);
            known = (acceptedExtensionNids.find(exceptionNid) != acceptedExtensionNids.end());
        }

        return known;
    }


    bool checkCriticalExtensions(X509& cert, const std::unordered_set<int>& acceptedExtensionNids)
    {
        for (int ind = X509_get_ext_by_critical(&cert, 1, -1);
             ind >= 0;
             ind = X509_get_ext_by_critical(&cert, 1, ind))
        {
            X509_EXTENSION* extension = X509_get_ext(&cert, ind);
            if (!validateExtensionKnown(extension, acceptedExtensionNids))
            {
                return false;
            }
        }

        return true;
    }

    std::string findStringInName(const X509_NAME& name, int nid, const std::regex& pattern)
    {
        int index = -1;
        // the look must end, because index is provided to the X509_NAME_get_index_by_NID
        // and thus there can not be more iterations than number of elements in the name
        for (;;)
        {
            // Find index within X509_NAME.
            index = X509_NAME_get_index_by_NID(&name, nid, index);
            if (index == -1)
            {
                return std::string{};
            }

            // Get the ASN.1 string for this field (not necessarily UTF-8).
            const X509_NAME_ENTRY* entryData = X509_NAME_get_entry(&name, index);
            Expect(entryData != nullptr, "can not get name entry");
            const ASN1_STRING* string = X509_NAME_ENTRY_get_data(entryData);
            Expect(string != nullptr, "can not get name entry data");

            // Convert the ASN.1 string to UTF-8.
            unsigned char* utf8Bytes = nullptr;
            auto size = ASN1_STRING_to_UTF8(&utf8Bytes, string);
            if (size < 0)
            {
                return std::string{};
            }
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            std::string candidateString{reinterpret_cast<char*>(utf8Bytes),
                                        gsl::narrow<std::string::size_type>(size)};
            OPENSSL_free(utf8Bytes);

            if (std::regex_match(candidateString, pattern))
            {
                return candidateString;
            }
        }
    }
}


X509Certificate::X509Certificate (X509* x509) : pCert{x509, X509_free}
{
}


X509Certificate X509Certificate::createFromBase64 (const std::string& string)
{
    return createFromAsnBytes(Base64::decode(string));
}


std::string X509Certificate::toBase64 () const
{
    if (!pCert)
    {
        return {};
    }

    unsigned char* buffer{nullptr};
    const int length{i2d_X509(pCert.get(), &buffer)};

    if (length < 0)
    {
        OPENSSL_free(buffer);
        Fail2("Unable to convert certificate to string.", CryptoFormalError);
    }
    const std::string& b64Encoded = Base64::encode(util::rawToBuffer(buffer, static_cast<size_t>(length)));
    OPENSSL_free(buffer);

    return b64Encoded;
}

X509Certificate X509Certificate::createFromPem(const std::string& pem)
{
    Certificate c = Certificate::fromPem(pem);
    X509* x509 = c.toX509().get();
    // Increase the ref count because our constructor doesn't.
    X509_up_ref(x509);
    return X509Certificate(x509);
}

std::string X509Certificate::toPem() const
{
    Expect(pCert != nullptr, "no X509 object");

    // Put the X509 in a shared_X509, so we can delegate to Certificate::toPemString(). We need to
    // increase the ref count since shared_X509::make() doesn't.
    auto sharedX509 = shared_X509::make(pCert.get());
    X509_up_ref(pCert.get());

    return Certificate{std::move(sharedX509)}.toPem();
}


X509Certificate
X509Certificate::createFromAsnBytes (gsl::span<const unsigned char> asnBytes)
{
    const unsigned char* myBuffer = asnBytes.data();
    X509* x509 = d2i_X509(nullptr, &myBuffer, gsl::narrow_cast<long>(asnBytes.size()));
    if (!x509)
    {
        Fail2("Unable to create certificate from der encoded data.", CryptoFormalError);
    }

    return X509Certificate(x509);
}


X509Certificate X509Certificate::createFromX509Pointer(X509* x509)
{
    OpenSslExpect(1 == X509_up_ref(x509), "Can not increase X509 reference.");
    return X509Certificate(x509);
}

// Useful X509Certificate information:
// We can add methods to parse the subject and issuer , e.g. by common name.
std::string X509Certificate::getSubject (void) const
{
    if (!pCert)
    {
        return {};
    }

    X509_name_st* subjectName = X509_get_subject_name(pCert.get());
    Expect(subjectName != nullptr, "can not get subject name");

    char* subject = X509_NAME_oneline(subjectName, nullptr, 0);
    Expect(subject != nullptr, "can not get subject name string");

    std::string retVal(subject);
    OPENSSL_free(subject);
    return retVal;
}


std::string X509Certificate::getIssuer (void) const
{
    if (!pCert)
    {
        return {};
    }

    X509_name_st* issuerName = X509_get_issuer_name(pCert.get());
    Expect(issuerName != nullptr, "can not get issuer name");

    char* issuer = X509_NAME_oneline(issuerName, nullptr, 0);
    Expect(issuer != nullptr, "can not get issuer name string");

    std::string retVal(issuer);
    OPENSSL_free(issuer);
    return retVal;
}


std::string X509Certificate::getFingerprint(const EVP_MD *(*digestFunction)(), const size_t digestLength) const
{
    if (!pCert)
    {
        return {};
    }

    std::unique_ptr<unsigned char[]> buf(new unsigned char[digestLength]);

    // This returns a ref that we do not need to delete.
    const EVP_MD* digest = digestFunction();
    Expect(digest != nullptr, "can not get digest");

    unsigned int len{};

    int rc = X509_digest(pCert.get(), digest, buf.get(), &len);
    if (rc == 0 || len != digestLength)
    {
        Fail2("Unable to calculate digest of certificate.", CryptoFormalError);
    }

    return ByteHelper::toHex({reinterpret_cast<const char*>(buf.get()),
                              gsl::narrow<std::size_t>(len)});
}


std::string X509Certificate::getSha256FingerprintHex(void) const
{
    return getFingerprint(EVP_sha256, 32);
}


std::string X509Certificate::getSha1FingerprintHex (void) const
{
    return getFingerprint(EVP_sha1, 20);
}

std::string X509Certificate::getSerialNumber() const
{
    if (!pCert)
    {
        return {};
    }

    const ASN1_INTEGER* serial = X509_get_serialNumber(pCert.get());
    Expect(serial != nullptr, "cannot get serial number");
    shared_BIO mem = shared_BIO::make();
    Expect(i2a_ASN1_INTEGER(mem, serial) > 0, "Conversion from serial to string failed");
    return bioToString(mem.get());
}

std::string X509Certificate::getSerialNumberFromSubject() const
{
    if (pCert == nullptr)
    {
        return {};
    }

    X509_NAME* name = X509_get_subject_name(pCert.get());
    if (name == nullptr)
    {
        Fail2("Could not retrieve X509 subject name", CryptoFormalError);
    }

    return findStringInName(*name, NID_serialNumber, notEmptyRegex);
}

bool X509Certificate::isCaCert (void) const
{
    return (pCert && X509_check_ca(pCert.get()) != 0);
}


std::string X509Certificate::getSubjectKeyIdentifier () const
{
    if (!pCert)
    {
        return {};
    }

    const auto* identifier{X509_get0_subject_key_id(pCert.get())};
    if (identifier)
    {
        return ByteHelper::toHex(
            {reinterpret_cast<const char*>(identifier->data),
             gsl::narrow<std::size_t>(identifier->length)});
    }
    return {};
}


std::string X509Certificate::getAuthorityKeyIdentifier () const
{
    if (!pCert)
    {
        return {};
    }

    const auto* identifier{X509_get0_authority_key_id(pCert.get())};
    if (identifier)
    {
        return ByteHelper::toHex(
            {reinterpret_cast<const char*>(identifier->data),
             gsl::narrow<std::size_t>(identifier->length)});
    }
    return {};
}


std::optional<std::string> X509Certificate::getTelematikId() const
{
    if (! pCert)
    {
        return std::nullopt;
    }

    for (auto extensionItr = 0; extensionItr < X509_get_ext_count(pCert.get());
         ++extensionItr)
    {
        auto* extension = X509_get_ext(pCert.get(), extensionItr);
        if (! extension)
        {
            continue;
        }

        const auto* extensionOidAsn1 = X509_EXTENSION_get_object(extension);
        if (! extensionOidAsn1)
        {
            continue;
        }

        // the +1/-1 tricks are only to please OpenSSL
        // OBJ_obj2txt feels very strong about writing an extra '\0' byte at the end of the buffer
        //
        util::Buffer extensionOid(teletrustAdmissionSyntaxOid.length() + 1);
        if (gsl::narrow<int>(extensionOid.size()) - 1
            != OBJ_obj2txt(
                reinterpret_cast<char*>(extensionOid.data()),
                gsl::narrow_cast<int>(extensionOid.size()),
                extensionOidAsn1,
                1))
        {
            continue;
        }

        if (teletrustAdmissionSyntaxOid
            != std::string{reinterpret_cast<char*>(extensionOid.data()), extensionOid.size() - 1})
        {
            continue;
        }

        // we have found the right extension, so at this point if there's a failure
        // prior to iterating through the admissions/professionInfos, then we need to
        // return nullopt because there's no hope to still find the telematik ID elsewhere
        return extractTelematikIdFromExtension(extension);
    }

    return std::nullopt;
}


bool X509Certificate::containsTelematikId(const std::string& telematikId) const
{
    if (!pCert)
    {
        return false;
    }

    for (auto extensionItr = 0; extensionItr < X509_get_ext_count(pCert.get()); ++extensionItr)
    {
        auto* extension = X509_get_ext(pCert.get(), extensionItr);
        if (!extension)
        {
            continue;
        }

        const auto* extensionOidAsn1 = X509_EXTENSION_get_object(extension);
        if (!extensionOidAsn1)
        {
            continue;
        }

        // the +1/-1 tricks are only to please OpenSSL
        // OBJ_obj2txt feels very strong about writing an extra '\0' byte at the end of the buffer
        //
        util::Buffer extensionOid(teletrustAdmissionSyntaxOid.length() + 1);
        if (gsl::narrow<int>(extensionOid.size()) - 1 != OBJ_obj2txt(reinterpret_cast<char*>(extensionOid.data()),
                                                                     gsl::narrow_cast<int>(extensionOid.size()),
                                                                     extensionOidAsn1,
                                                                     1))
        {
            continue;
        }

        if (teletrustAdmissionSyntaxOid != std::string{reinterpret_cast<char*>(extensionOid.data()),
                                                       extensionOid.size() - 1})
        {
            continue;
        }

        // we have found the right extension, so at this point if there's a failure
        // prior to iterating through the admissions/professionInfos, then we need to
        // return false because there's no hope to still find the telematik ID elsewhere
        return checkExtension(extension, telematikId);
    }

    return false;
}


// "X.509-Identitäten für die Erstellung und Prüfung digitaler Signaturen"
// GEMREQ-start GS-A_4361-02
bool X509Certificate::signatureIsValidAndWasSignedBy (const X509Certificate& issuer) const
{
    // "X.509-Identitäten für die Erstellung und Prüfung digitaler nicht-qualifizierter elektronischer Signaturen"
    // GEMREQ-start GS-A_4357
    if (!pCert)
    {
        return false;
    }

    EVP_PKEY* publicKeyOfIssuer = X509_get0_pubkey(issuer.pCert.get());
    return (publicKeyOfIssuer && X509_verify(pCert.get(), publicKeyOfIssuer) == 1);
    // GEMREQ-end GS-A_4357
}
// GEMREQ-end GS-A_4361-02


bool X509Certificate::checkValidityPeriod (std::chrono::system_clock::time_point when) const
{
    if (!pCert)
    {
        return false;
    }

    // get validity period from certificate
    const auto* notBefore = X509_get0_notBefore(pCert.get());
    if (!notBefore)
    {
        Fail2("Unable to retrieve Not Valid Before time from certificate.", CryptoFormalError);
    }
    const auto* notAfter = X509_get0_notAfter(pCert.get());
    if (!notAfter)
    {
        Fail2("Unable to retrieve Not Valid After time from certificate.", CryptoFormalError);
    }

    // compare to now
    const std::time_t now = std::chrono::system_clock::to_time_t(when);
    const int comparedToNotBefore = ASN1_TIME_cmp_time_t(notBefore, now);
    const int comparedToNotAfter = ASN1_TIME_cmp_time_t(notAfter, now);

    return !(comparedToNotBefore == 1 ||  // now < notBefore
             comparedToNotAfter == -1 ||  // notAfter < now
             comparedToNotBefore == -2 || // error in first comparison
             comparedToNotAfter == -2);
}


bool X509Certificate::hasX509Ptr () const
{
    return pCert != nullptr;
}


const X509* X509Certificate::getX509ConstPtr() const
{
    return pCert.get();
}


X509* X509Certificate::getX509Ptr()
{
    return pCert.get();
}


SigningAlgorithm X509Certificate::getSigningAlgorithm () const
{
    EVP_PKEY* pKey = getPublicKey();

    if (pKey)
    {
        const int baseId = EVP_PKEY_base_id(pKey);
        if (baseId == EVP_PKEY_RSA || baseId == EVP_PKEY_RSA2 || baseId == EVP_PKEY_RSA_PSS)
        {
            return SigningAlgorithm::rsaPss;
        }
    }

    return SigningAlgorithm::ellipticCurve;
}


EVP_PKEY* X509Certificate::getPublicKey () const
{
    if (!pCert)
    {
        return nullptr;
    }

    return X509_get0_pubkey(pCert.get());
}


std::unique_ptr<EllipticCurvePublicKey> X509Certificate::getEllipticCurvePublicKey() const
{
    if (SigningAlgorithm::ellipticCurve != getSigningAlgorithm())
    {
        Fail2("There is no elliptic curve public key to be retrieved", std::logic_error);
    }

    auto* evpKey = getPublicKey();
    if (evpKey)
    {
        const auto* ecKey = EVP_PKEY_get0_EC_KEY(evpKey);
        if (ecKey)
        {
            const auto* ecPublicKey = EC_KEY_get0_public_key(ecKey);
            return std::make_unique<EllipticCurvePublicKey>(NID_brainpoolP256r1, ecPublicKey);
        }
    }

    Fail2("Cannot retrieve elliptic curve public key", std::runtime_error);
}


X509Certificate::IdentifierInformation X509Certificate::extractIdentifierInformation () const
{
    if (pCert == nullptr)
    {
        return {};
    }

    X509_NAME* name = X509_get_subject_name(pCert.get());
    if (name == nullptr)
    {
        Fail2("Could not retrieve X509 subject name", CryptoFormalError);
    }

    static const std::regex kvnrRegex{"[A-Z]{1}[0-9]{9}"};
    static const std::regex institutionIdRegex{"[0-9]{9}"};

    // These three fields must be filled according to gemSpec_PK_eG_V1.2.0, item Card-G2-A_3853
    // organizationalUnitName, givenName, surname
    return {
        findStringInName(*name, NID_organizationalUnitName, kvnrRegex),
        findStringInName(*name, NID_givenName, notEmptyRegex),
        findStringInName(*name, NID_surname, notEmptyRegex),
        findStringInName(*name, NID_organizationalUnitName, institutionIdRegex)};
}

std::vector<std::string> X509Certificate::getOcspUrls() const {

    if (!pCert)
    {
        return {};
    }

    std::vector<std::string> result;

    STACK_OF(OPENSSL_STRING) *ocspStack = X509_get1_ocsp(pCert.get());
    Expect(ocspStack != nullptr, "can not get ocsp stack");
    int len = sk_OPENSSL_STRING_num(ocspStack);
    OpenSslExpect(len >= 0, "sk_OPENSSL_STRING_num failed");
    result.reserve(static_cast<size_t>(len));
    for (int i = 0; i < len; ++i)
    {
        result.emplace_back(sk_OPENSSL_STRING_value(ocspStack, i));
    }

    X509_email_free(ocspStack);
    return result;
}


bool X509Certificate::checkRoles (const std::vector<std::string>& roleOids) const {
    if (!pCert)
    {
        return false;
    }

    // x509v3 extension admission is of type ADMISSION_SYNTAX
    int criticality = 0, extIndex = -1;
    const std::shared_ptr<ADMISSION_SYNTAX> admissionSyntax{
            static_cast<ADMISSION_SYNTAX *>(X509_get_ext_d2i(pCert.get(), NID_x509ExtAdmission,
                                                             &criticality,
                                                             &extIndex)), ADMISSION_SYNTAX_free};

    if (criticality == -1 || !admissionSyntax)
    {
        return false;
    }

    // ADMISSION_SYNTAX has a stack of ADMISSION items
    const STACK_OF(ADMISSIONS) *admissions{
            ADMISSION_SYNTAX_get0_contentsOfAdmissions(admissionSyntax.get())};
    if (!admissions)
    {
        return false;
    }

    std::vector<std::shared_ptr<ASN1_OBJECT>> objectsToSearchFor;
    objectsToSearchFor.reserve(roleOids.size());

    for (const std::string &roleOid: roleOids)
    {
        std::shared_ptr<ASN1_OBJECT> objectToSearchFor{OBJ_txt2obj(roleOid.c_str(), 1), ASN1_OBJECT_free};
        if (objectToSearchFor != nullptr)
            objectsToSearchFor.push_back(objectToSearchFor);
    }

    // no valid oids
    if (objectsToSearchFor.empty())
    {
        return false;
    }

    return checkRoles(objectsToSearchFor, admissions);
}

bool X509Certificate::checkRoles(const std::vector<std::shared_ptr<ASN1_OBJECT>>& roleOids,
                                 const STACK_OF(ADMISSIONS)* admissions)
{
    const int numberOfAdmissions{sk_ADMISSIONS_num(admissions)};
    for (int i = 0; i < numberOfAdmissions; ++i)
    {
        const ADMISSIONS* admission{sk_ADMISSIONS_value(admissions, i)};
        if (!admission)
        {
            continue;
        }
        const STACK_OF(PROFESSION_INFO)* professionInfos{ADMISSIONS_get0_professionInfos(admission)};
        if (checkRoles(roleOids, professionInfos))
        {
            return true;
        }
    }
    return false;
}

bool X509Certificate::checkRoles(const std::vector<std::shared_ptr<ASN1_OBJECT>>& roleOids,
                                 const STACK_OF(PROFESSION_INFO)* professionInfos)
{
    const int numberOfProfessionInfos{sk_PROFESSION_INFO_num(professionInfos)};
    for (int j = 0; j < numberOfProfessionInfos; ++j)
    {
        const PROFESSION_INFO* professionInfo{sk_PROFESSION_INFO_value(professionInfos, j)};
        if (!professionInfo)
        {
            continue;
        }
        const STACK_OF(ASN1_OBJECT)* professionOids{PROFESSION_INFO_get0_professionOIDs(professionInfo)};
        if (checkRoles(roleOids, professionOids))
        {
            return true;
        }
    }
    return false;
}

bool X509Certificate::checkRoles(const std::vector<std::shared_ptr<ASN1_OBJECT>>& roleOids,
                                 const STACK_OF(ASN1_OBJECT)* professionOids)
{
    const int numberOfProfessionOids{sk_ASN1_OBJECT_num(professionOids)};
    for (int k = 0; k < numberOfProfessionOids; ++k)
    {
        const ASN1_OBJECT* professionOid{sk_ASN1_OBJECT_value(professionOids, k)};
        if (!professionOid)
        {
            continue;
        }
        for (const auto& objectToSearchFor: roleOids)
        {
            if (OBJ_cmp(professionOid, objectToSearchFor.get()) == 0)
            {
                return true;
            }
        }
    }

    return false;
}


bool X509Certificate::checkKeyUsage (const std::vector<KeyUsage>& keyUsages) const
{
    if (!pCert)
    {
        return false;
    }

    int criticality = 0, extIndex = -1;
    std::shared_ptr<ASN1_BIT_STRING> keyUsage{
        static_cast<ASN1_BIT_STRING*>(X509_get_ext_d2i(pCert.get(), NID_key_usage, &criticality,
                                                       &extIndex)), ASN1_BIT_STRING_free};
    if (criticality == -1 || !keyUsage)
    {
        return false;
    }

    for (const auto& keyUsageBit: keyUsages)         // NOLINT(readability-use-anyofallof)
    {
        if (ASN1_BIT_STRING_get_bit(keyUsage.get(), static_cast<int>(keyUsageBit)) == 0)
        {
            return false;
        }
    }

    return true;
}



bool X509Certificate::checkExtendedKeyUsage(const std::vector<ExtendedKeyUsage> &extendedKeyUsages) const
{
    if (!pCert)
    {
        return false;
    }

    if ((X509_get_extension_flags(pCert.get()) & EXFLAG_XKUSAGE) == EXFLAG_XKUSAGE)
    {
        uint32_t matchMask = 0x0;
        for (const ExtendedKeyUsage extendedKeyUsage: extendedKeyUsages)
        {
            matchMask |= static_cast<uint32_t>(extendedKeyUsage);
        }

        const uint32_t certificate_key_usage = X509_get_extended_key_usage(pCert.get());
        return (matchMask & certificate_key_usage) == certificate_key_usage;
    }
    else
    {
        return extendedKeyUsages.empty();
    }
}


bool X509Certificate::checkCertificatePolicy (const std::string& policyOid) const
{
    if (!pCert)
    {
        return false;
    }

    // x509v3 extension certificate policies is of type CERTIFICATEPOLICIES, which is a
    // STACKOF(POLICIINFO)
    int criticality = 0, extIndex = -1;
    std::shared_ptr<CERTIFICATEPOLICIES> certificatePolicies{
        static_cast<CERTIFICATEPOLICIES*>(X509_get_ext_d2i(pCert.get(), NID_certificate_policies,
                                                           &criticality, &extIndex)),
        CERTIFICATEPOLICIES_free};

    if (criticality == -1 || !certificatePolicies)
    {
        return false;
    }

    // POLICYINFO is a struct with member policyid, which points to the ASN1_OBJECT representing
    // the policy oid
    const int numberOfPolicies{sk_POLICYINFO_num(certificatePolicies.get())};
    std::shared_ptr<ASN1_OBJECT> objectToSearchFor{OBJ_txt2obj(policyOid.c_str(), 1),
                                                   ASN1_OBJECT_free};
    // not a valid oid
    if (!objectToSearchFor)
    {
        return false;
    }

    for (int i = 0; i < numberOfPolicies; ++i)
    {
        POLICYINFO* policyInfo = sk_POLICYINFO_value(certificatePolicies.get(), i);
        if (policyInfo->policyid)
        {
        }
        if (policyInfo->policyid &&
            OBJ_cmp(policyInfo->policyid, objectToSearchFor.get()) == 0)
        {
            return true;
        }
    }

    return false;
}


bool X509Certificate::hasCertificatePolicy () const
{
    if (!pCert)
    {
        return false;
    }

    // x509v3 extension certificate policies is of type CERTIFICATEPOLICIES, which is a
    // STACKOF(POLICIINFO)
    int criticality = 0, extIndex = -1;
    std::shared_ptr<CERTIFICATEPOLICIES> certificatePolicies{
        static_cast<CERTIFICATEPOLICIES*>(X509_get_ext_d2i(pCert.get(), NID_certificate_policies,
                                                           &criticality, &extIndex)),
        CERTIFICATEPOLICIES_free};

    return (criticality > -1 && certificatePolicies != nullptr);
}


bool X509Certificate::checkQcStatement(const std::string& qcStatementOid) const
{
    if (!pCert)
    {
        return false;
    }

    return (getQcStatement(*pCert, qcStatementOid.size() + 1) == qcStatementOid);
}


bool X509Certificate::checkCritical(const std::unordered_set<int>& acceptedExtensionNids) const
{
    if (!pCert)
    {
        return false;
    }

    return checkCriticalExtensions(*pCert, acceptedExtensionNids);
}


std::vector<std::string> X509Certificate::getRoles () const
{
    std::vector<std::string> roleOids;

    if (!pCert)
    {
        return roleOids;
    }

    // x509v3 extension admission is of type ADMISSION_SYNTAX
    int criticality = 0, extIndex = -1;
    const std::shared_ptr<ADMISSION_SYNTAX> admissionSyntax{
        static_cast<ADMISSION_SYNTAX*>(X509_get_ext_d2i(pCert.get(), NID_x509ExtAdmission,
                                                        &criticality,
                                                        &extIndex)), ADMISSION_SYNTAX_free};

    if (criticality == -1 || !admissionSyntax)
    {
        return roleOids;
    }

    // ADMISSION_SYNTAX has a stack of ADMISSION items
    const STACK_OF(ADMISSIONS)* admissions{
        ADMISSION_SYNTAX_get0_contentsOfAdmissions(admissionSyntax.get())};
    if (!admissions)
    {
        return roleOids;
    }
    appendRoles(roleOids, admissions);
    return roleOids;
}

void X509Certificate::appendRoles(std::vector<std::string>& outRoleOids, const STACK_OF(ADMISSIONS)* admissions)
{
    const int numberOfAdmissions{sk_ADMISSIONS_num(admissions)};
    for (int i = 0; i < numberOfAdmissions; ++i)
    {
        const ADMISSIONS* admission{sk_ADMISSIONS_value(admissions, i)};

        // ADMISSION has a stack of PROFESSION_INFO items
        const STACK_OF(PROFESSION_INFO)* professionInfos{
            ADMISSIONS_get0_professionInfos(admission)};
        appendRoles(outRoleOids, professionInfos);
    }
}

void X509Certificate::appendRoles(std::vector<std::string>& outRoleOids, const STACK_OF(PROFESSION_INFO)* professionInfos)
{
    const int numberOfProfessionInfos{sk_PROFESSION_INFO_num(professionInfos)};
    for (int j = 0; j < numberOfProfessionInfos; ++j)
    {
        const PROFESSION_INFO* professionInfo{sk_PROFESSION_INFO_value(professionInfos, j)};
        if (!professionInfo)
        {
            continue;
        }

        // PROFESSION_INFO has a stack of profession OIDs
        const STACK_OF(ASN1_OBJECT)* professionOids{
            PROFESSION_INFO_get0_professionOIDs(professionInfo)};
        appendRoles(outRoleOids, professionOids);
    }
}

void X509Certificate::appendRoles(std::vector<std::string>& outRoleOids, const STACK_OF(ASN1_OBJECT)* professionOids)
{
    const int numberOfProfessionOids{sk_ASN1_OBJECT_num(professionOids)};
    for (int k = 0; k < numberOfProfessionOids; ++k)
    {
        const ASN1_OBJECT* professionOid{sk_ASN1_OBJECT_value(professionOids, k)};
        if (!professionOid)
        {
            continue;
        }

        // 'A buffer length of 80 should be more than enough to handle any OID encountered
        // in practice.' (from the man page for OBJ_obj2txt)
        static constexpr const int bufferLength{80};
        char buffer[bufferLength];

        // last parameter (no_name) == 1: always use numerical form
        const int stringLength{OBJ_obj2txt(buffer, bufferLength, professionOid, 1)};
        if (stringLength > 0)
        {
            outRoleOids.emplace_back(buffer, stringLength);
        }
    }
}

tm X509Certificate::getNotAfter() const
{
    Expect(pCert, "pCert is null");
    const auto* notAfter = X509_get0_notAfter(pCert.get());
    if (!notAfter)
    {
        Fail2("Unable to retrieve Not Valid After time from certificate.", CryptoFormalError);
    }
    tm notAfterTm{};
    OpenSslExpect(ASN1_TIME_to_tm(notAfter, &notAfterTm), "ASN1_TIME_to_tm failed for notAfter");
    return notAfterTm;
}


bool X509Certificate::operator==(const X509Certificate& rhs) const
{
    return pCert == rhs.pCert ||
           (pCert != nullptr && rhs.pCert != nullptr && 0 == X509_cmp(getX509ConstPtr(), rhs.getX509ConstPtr()));
}


bool X509Certificate::operator!=(const X509Certificate& rhs) const
{
    return ! operator==(rhs);
}


std::unordered_multimap<std::string, std::string> X509Certificate::getDistinguishedNameMapFromSubject() const
{
    if (!pCert)
    {
        return {};
    }

    X509_NAME *subj = X509_get_subject_name(pCert.get());

    std::unordered_multimap<std::string, std::string> dn;

    for (int i = 0; i < X509_NAME_entry_count(subj); i++) {
        X509_NAME_ENTRY *entry = X509_NAME_get_entry(subj, i);
        Expect(entry != nullptr, "can not retrieve name entry");

        ASN1_OBJECT *nameObject = X509_NAME_ENTRY_get_object(entry);
        Expect(nameObject != nullptr, "can not retrieve name entry object");

        int nid = OBJ_obj2nid(nameObject);
        const char * key = OBJ_nid2sn(nid);
        ASN1_STRING *v = X509_NAME_ENTRY_get_data(entry);
        if (key != nullptr && v != nullptr)
        {
            dn.insert({std::string(key), std::string(reinterpret_cast<const char*>(v->data), gsl::narrow<size_t>(v->length))});
        }
    }

    return dn;
}


std::vector<std::string> X509Certificate::getSubjectAlternativeNameDnsNames() const
{
    std::vector<std::string> result;

    int index = -1;
    while (auto* subjectAltName = static_cast<GENERAL_NAMES*>(
               X509_get_ext_d2i(getX509ConstPtr(), NID_subject_alt_name, nullptr, &index)))
    {
        std::unique_ptr<GENERAL_NAMES, decltype(&GENERAL_NAMES_free)> subjectAltNameDeleter{
            subjectAltName, &GENERAL_NAMES_free};

        for (int i = 0; i < sk_GENERAL_NAME_num(subjectAltName); ++i)
        {
            GENERAL_NAME* generalName = sk_GENERAL_NAME_value(subjectAltName, i);
            if (generalName->type != GEN_DNS)
                continue;

            std::string name = asn1StringToUtf8(generalName->d.dNSName);
            if (! name.empty())
                result.push_back(std::move(name));
        }
    }

    return result;
}

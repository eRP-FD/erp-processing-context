/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TSL_X509CERTIFICATE_HXX
#define ERP_PROCESSING_CONTEXT_TSL_X509CERTIFICATE_HXX

#include "fhirtools/util/Gsl.hxx"
#include "shared/crypto/OpenSsl.hxx"

#include <chrono>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

typedef struct x509_st X509;
typedef struct evp_pkey_st EVP_PKEY;

class EllipticCurvePublicKey;


/**
 * Possible values for key usage extension.
 * @see RFC 5280 §4.2.1.3 (https://tools.ietf.org/html/rfc5280#section-4.2.1.3)
 */
enum class KeyUsage
{
    digitalSignature = 0,
    nonRepudiation = 1,
    keyEncipherment = 2,
    dataEncipherment = 3,
    keyAgreement = 4,
    keyCertSign = 5,
    cRLSign = 6,
    encipherOnly = 7,
    decipherOnly = 8
};

enum class ExtendedKeyUsage
{
    sslServer = 0x1,
    sslClient = 0x2,
    smime = 0x4,
    codeSign = 0x8,
    sgc = 0x10,
    ocspSign = 0x20,
    timestamp = 0x40,
    dvcs = 0x80,
    anyEku = 0x100
};


enum class SigningAlgorithm
{
    rsaPss,
    ellipticCurve
};


class X509Certificate
{
protected:
    /**
     * Initializes X509Certificate object with X509 pointer,
     * important: the ref count of the pointer is _not_ increased.
     */
    explicit X509Certificate (X509* x509);

public:
    /**
     * Represents identifier information from the subject name of the certificate
     */
    class IdentifierInformation
    {
    public:
        /**
         * person id if it is provided in organizationalUnitName
         * that must be filled according to
         * gemSpec_PK_eG_V1.2.0, item Card-G2-A_3853
         */
        std::string personId;

        /**
         * givenName, must be filled according to
         * gemSpec_PK_eG_V1.2.0, item Card-G2-A_3853
         */
        std::string givenName;

        /**
         * surname, must be filled according to
         * gemSpec_PK_eG_V1.2.0, item Card-G2-A_3853
         */
        std::string surname;

        /**
         * institution id if it is provided in organizationalUnitName
         * that must be filled according to
         * gemSpec_PK_eG_V1.2.0, item Card-G2-A_3853
         */
        std::string institutionId;
    };

    X509Certificate () = default;

    /**
     * Creates a X509Certificate from the base 64 encoded, DER formatted X509 certificate.
     * @param string the certificate in DER format, base 64 encoded
     * @return the X509Certificate object
     * @throws std::exception on error
     */
    static X509Certificate createFromBase64 (const std::string& string);

    std::string toBase64() const;

    /**
     * Creates a X509Certificate from a PEM file.
     *
     * This is similar to createFromBase64, but removes the PEM header/footer and all line breaks.
     */
    static X509Certificate createFromPem(const std::string& pem);

    std::string toPem() const;

    /**
    * Creates a X509Certificate from a ASN.1/DER encoded X509 certificate.
    * @param asnBytes the certificate in DER format
    * @return the X509Certificate object
    * @throws std::exception on error
    */
    static X509Certificate createFromAsnBytes (gsl::span<const unsigned char> asnBytes);

    /**
     * Creates a X509Certificate from a X509 pointer, the refcount is increased.
     * @param x509 the pointer to X509 object
     * @return the X509Certificate object
     * @throws std::exception on error
     */
    static X509Certificate createFromX509Pointer(X509* x509);

    std::string getSubject (void) const;

    std::string getIssuer (void) const;

    std::string getSha1FingerprintHex (void) const;

    std::string getSha256FingerprintHex (void) const;

    /**
     * Get the hex encoded serial number of the certificate
     */
    std::string getSerialNumber() const;

    /**
     * Get the serial number from the subject
     */
    std::string getSerialNumberFromSubject() const;

    bool isCaCert (void) const;

    std::string getSubjectKeyIdentifier () const;

    std::string getAuthorityKeyIdentifier () const;

    std::optional<std::string> getTelematikId() const;

    bool containsTelematikId(const std::string& telematikId) const;

    bool signatureIsValidAndWasSignedBy (const X509Certificate& issuer) const;

    bool checkValidityPeriod (
        std::chrono::system_clock::time_point when = std::chrono::system_clock::now()) const;

    bool hasX509Ptr () const;

    const X509* getX509ConstPtr() const;
    X509* getX509Ptr();

    SigningAlgorithm getSigningAlgorithm () const;

    /**
     * Returns a pointer to the public key of the X509 certificate.
     *
     * This method does <b>not</b> increment the reference count of the returned EVP_PKEY, so it
     * must not be freed up after use.
     */
    EVP_PKEY* getPublicKey () const;

    /**
     * Returns an unique pointer to the elliptic curve public key of the X509 certificate.
     *
     * Throws if the signing algorithm is not ellipticCurve.
     */
    std::unique_ptr<EllipticCurvePublicKey> getEllipticCurvePublicKey() const;

    /**
     * Returns a tuple of:
     * - Person object with the following fields filled: identifier (KVNR/record ID),
     *   givenName, familyName.
     * - institutionID in the form of an std::string
     */
    IdentifierInformation extractIdentifierInformation () const;

    /**
     * Returns the OCSP validation endpoint urls
     * @return vector of url strings
     */
    std::vector<std::string> getOcspUrls() const;

    /**
     * Checks, if the admission extension of the certificate contains at least one of the given roles.
     *
     * @param roleOids    the oids of the roles
     */
    bool checkRoles (const std::vector<std::string>& roleOids) const;

    /**
     * Checks, if the key usage extension is available for the certificate and
     * if a list of bits are set for this extension.
     *
     * @param keyUsages   the list of bits
     */
    bool checkKeyUsage (const std::vector<KeyUsage>& keyUsages) const;

    /**
     * Checks, if the extended key usage extension is available for the certificate and
     * if a list of bits are set for this extension.
     *
     * @param extendedKeyUsages   the list of bits
     */
    bool checkExtendedKeyUsage (const std::vector<ExtendedKeyUsage> &extendedKeyUsages) const;

    /**
     * Checks, if the certificate policies extension of the certificate contains a given policy.
     * @param policyOid   the oid of the policy
     */
    bool checkCertificatePolicy (const std::string& policyOid) const;

    /**
     * Checks, if the certificate policies extension is provided in the certificate.
     */
    bool hasCertificatePolicy () const;

    /**
     * Checks, if the certificate QCStatements contain at least one entry with specified oid.
     * @param qcStatementOid the oid of QCStatement
     */
    bool checkQcStatement(const std::string& qcStatementOid) const;

    /**
     * Checks, if all the certificate critical extensions are in the collection of accepted critical extensions.
     * @param acceptedExtensionNids     the accepted critical extension nids.
     */
    bool checkCritical(const std::unordered_set<int>& acceptedExtensionNids) const;

    /**
     * Retrieves all role OIDs from the certificate.
     *
     * @return the role OIDs in numerical form
     * @see gemSpec_PKI, § 8.3.3.2 TUC_PKI_009 "Rollenermittlung"
     */
    std::vector<std::string> getRoles () const;

    [[nodiscard]] tm getNotAfter() const;

    bool operator==(const X509Certificate& rhs) const;
    bool operator!=(const X509Certificate& rhs) const;

    std::unordered_multimap<std::string, std::string> getDistinguishedNameMapFromSubject() const;

    /**
     * Returns all names of type DNS from the subject alternative name extension.
     */
    std::vector<std::string> getSubjectAlternativeNameDnsNames() const;

private:
    static bool checkRoles (const std::vector<std::shared_ptr<ASN1_OBJECT>>& roleOids,
                            const STACK_OF(ADMISSIONS)* admissions);
    static bool checkRoles (const std::vector<std::shared_ptr<ASN1_OBJECT>>& roleOids,
                            const STACK_OF(PROFESSION_INFO)* professionInfos);
    static bool checkRoles (const std::vector<std::shared_ptr<ASN1_OBJECT>>& roleOids,
                            const STACK_OF(ASN1_OBJECT)* professionOids);
    static void appendRoles(std::vector<std::string>& outRoleOids, const STACK_OF(ADMISSIONS)* admissions);
    static void appendRoles(std::vector<std::string>& outRoleOids, const STACK_OF(PROFESSION_INFO)* professionInfos);
    static void appendRoles(std::vector<std::string>& outRoleOids, const STACK_OF(ASN1_OBJECT)* professionOids);


    using RawCertificatePtr = std::shared_ptr<X509>;
    std::string getFingerprint(const EVP_MD *(*digestFunction)(), const size_t digestLength) const;

    RawCertificatePtr pCert;
};

#endif

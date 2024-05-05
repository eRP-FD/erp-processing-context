/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TSL_TSLSERVICE_HXX
#define ERP_PROCESSING_CONTEXT_TSL_TSLSERVICE_HXX

#include "erp/client/UrlRequestSender.hxx"
#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/tsl/OcspCheckDescriptor.hxx"
#include "erp/tsl/X509Certificate.hxx"

#include <optional>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>


class HttpsClient;

class OcspResponse;
class TslParser;

class TrustStore;

class X509Certificate;

class XmlValidator;

enum class CertificateType
{
    C_CH_AUT,
    C_CH_AUT_ALT,
    C_FD_AUT,
    C_FD_SIG,
    C_FD_OSIG,
    C_HCI_ENC,
    C_HCI_AUT,
    C_HCI_OSIG,
    C_HP_QES,
    C_CH_QES,

    /**
     * The following certificate type is introduced to comply with updated requirement A_20159-*
     */
    C_HP_ENC
};

/// This class implements an interface to the Trustservice Status List (TSL), which is an XML file
/// that contains information about trusted services (certificates).
class TslService
{
public:
    // OIDs
    // names and values have been taken from gemSpec_OID_V3.7.0.pdfs

    // C.CH.QES
    static constexpr const char* oid_egk_qes                           {"1.2.276.0.76.4.66"};
    // C.CH.AUT
    static constexpr const char* oid_egk_aut                           {"1.2.276.0.76.4.70"};
    // C.CH.AUT_ALT
    static constexpr const char* oid_egk_aut_alt                       {"1.2.276.0.76.4.212"};
    // C.FD.AUT
    static constexpr const char* oid_fd_aut                            {"1.2.276.0.76.4.155"};
    static constexpr const char* oid_epa_vau                           {"1.2.276.0.76.4.209"};
    // C.FD.SIG
    static constexpr const char* oid_fd_sig                            {"1.2.276.0.76.4.203"};
    static constexpr const char* oid_fd_osig                           {"1.2.276.0.76.4.283"};
    static constexpr const char* oid_epa_authn                         {"1.2.276.0.76.4.204"};
    static constexpr const char* oid_epa_authz                         {"1.2.276.0.76.4.205"};
    static constexpr const char* oid_wadg                              {"1.2.276.0.76.4.198"};
    // C.HCI.ENC
    static constexpr const char* oid_smc_b_enc                         {"1.2.276.0.76.4.76"};
    // C.HCI.AUT
    static constexpr const char* oid_smc_b_aut                         {"1.2.276.0.76.4.77"};
    // C.HCI.OSIG
    static constexpr const char* oid_smc_b_osig                        {"1.2.276.0.76.4.78"};
    // C.HP.QES
    static constexpr const char* oid_hba_qes                           {"1.2.276.0.76.4.72"};
    // C_HP_ENC
    static constexpr const char* oid_vk_eaa_enc                        {"1.3.6.1.4.1.24796.1.10"};

    // QC-Statement-Types are described in ETSI EN 319 412-5
    static constexpr const char* id_etsi_qcs_QcCompliance              {"0.4.0.1862.1.1"};


    /**
     * This implementation does verification including OCSP-Validation of the provided certificate.
     * Info: According to gemSpec_PKI the checked certificates must be signed directly by the trusted authority.
     *
     * @param certificate               the certificate to check
     * @param typeRestrictions          if the set is not empty, the certificate type must be included there
     * @param requestSender             used to send OCSP status requests
     * @param trustStore                where to look for the certificate; may be updated
     *                                  during this call
     * @param ocspCheckDescriptor       describes the OCSP check approach to use
     *
     * @throws TslError                 in case of problems
     * @return OCSP-Response used to verify Certificate-Status
     */
    static OcspResponse checkCertificate(
        X509Certificate& certificate,
        const std::unordered_set<CertificateType>& typeRestrictions,
        const UrlRequestSender& requestSender,
        TrustStore& trustStore,
        const OcspCheckDescriptor& ocspCheckDescriptor);


    /**
     * Allows to initialise TI trust space and to update it.
     *
     * Must be called by every method that validates certificates for the case of outdated TI trust space.
     *
     * @param requestSender     used to download the new TSL file and its hash, and to send OCSP status requests
     * @param xmlValidator    used to get schema for xml validation of TSL files
     * @param trustStore        the trust store containing the information of the TSL
     *                          currently in use
     * @param onlyOutdated      update only if TSL is outdated
     * @param expectedSignerCertificate optional parameter, if it is provided, it must be the TSL-signer certificate
     *
     * @see gemSpec_PKI § 8.1.2.1 TIC_PKI_001: Periodische Aktualisierung TI-Vertrauensraum.
     *
     * @see gemSpec_Frontend_Vers § 6.1.5.2 TSL-Behandlung
     *
     * @throws TslError                 in case of problems
     */
    // GEMREQ-start A_15874
    enum class UpdateResult
    {
        Updated,
        NotUpdated
    };
    static UpdateResult triggerTslUpdateIfNecessary(
        UrlRequestSender& requestSender,
        const XmlValidator& xmlValidator,
        TrustStore& trustStore,
        const bool onlyOutdated,
        const std::vector<X509Certificate>& expectedSignerCertificates = {});
    // GEMREQ-end A_15874

    static CertificateType getCertificateType(const X509Certificate& certificate);

private:

    // gemSpec_PKI § 8.2.2.3 TUC_PKI_011: Prüfung des TSL-Signer-Zeritifikates
    // GEMREQ-start GS-A_4650
    static void checkSignerCertificate (
        const X509Certificate& signerCertificate,
        TrustStore& trustStore,
        const std::vector<X509Certificate>& expectedSignerCertificates);
    // GEMREQ-end GS-A_4650

    static void checkTslSignerExtendedKeyUsage(const X509Certificate& certificate);

    /**
     * Checks whether a new TSL file is available via download.  If so, it downloads this file,
     * validates it and checks if it is newer than the TSL used by the system.
     *
     * @see gemSpec_PKI § 8.2.2.1 TUC_PKI_019: Prüfung der Aktualität der TSL
     *
     * @return  if there an update was done TslParser with the contents of the new TSL, otherwise std::nullopt.
     *
     * @throws TslError                 in case of problems
     */
    // GEMREQ-start GS-A_4648
    static std::optional<TslParser> refreshTslIfNecessary(
        UrlRequestSender& requestSender,
        const XmlValidator& xmlValidator,
        TrustStore& trustStore,
        const std::optional<std::string>& newHash,
        const std::vector<X509Certificate>& expectedSignerCertificates);
    // GEMREQ-end GS-A_4648

    /**
     * Starts parsing of the new TSL and reports errors with exceptions.
     *
     * @return      if there an update was done TslParser with the contents of the new TSL, otherwise std::nullopt.
     *
     * @throws TslError                 in case of problems
     */
    static std::optional<TslParser> attemptTslParsing (
        const std::string& tslXml,
        const XmlValidator& xmlValidator,
        TrustStore& trustStore,
        const std::vector<X509Certificate>& expectedSignerCertificates);

    /**
     * This internal implementation does verification of the provided certificate without OCSP-Validation.
     * Info: According to gemSpec_PKI the checked certificates must be signed directly by the trusted authority.
     *
     * @param certificate               the certificate to check
     * @param typeRestrictions          if the set is not empty, the certificate type must be included there
     * @param trustStore                where to look for the certificate; may be updated
     *                                  during this call
     *
     * @throws TslError                 in case of problems
     */
    static std::tuple<CertificateType, X509Certificate>
    checkCertificateWithoutOcspCheck(
        X509Certificate& certificate,
        const std::unordered_set<CertificateType>& typeRestrictions,
        TrustStore& trustStore);


    // Make private methods available to gtest.
#ifdef FRIEND_TEST
    FRIEND_TEST(TslServiceTest, TslDownloadWithEmptyStoreWillBeUpdated);
    FRIEND_TEST(TslServiceTest, DownloadedNewHashValueIsNotEmpty);
    FRIEND_TEST(TslServiceTest, EmptyTruststoreIsUpdatedOnBoxEnv);
    FRIEND_TEST(TslServiceTest, DISABLED_EmptyTruststoreIsUpdatedOnDevEnv);
    FRIEND_TEST(EnvironmentHealthCheckTests, InitialTslUpdate);
    FRIEND_TEST(EnvironmentHealthCheckTests, OcspRequestForInsurantCertificate);
    FRIEND_TEST(ConfigurableLoadTests, InitialTslUpdate);
    FRIEND_TEST(ConfigurableLoadTests, OcspRequestForInsurantCertificate);
#endif
};

#endif

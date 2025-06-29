/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/tsl/TslService.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/network/client/UrlRequestSender.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/model/ProfessionOid.hxx"
#include "shared/tsl/OcspCheckDescriptor.hxx"
#include "shared/tsl/OcspService.hxx"
#include "shared/tsl/OcspUrl.hxx"
#include "shared/tsl/TrustStore.hxx"
#include "shared/tsl/TslParser.hxx"
#include "shared/tsl/X509Certificate.hxx"
#include "shared/tsl/error/TslError.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/GLog.hxx"
#include "shared/util/Hash.hxx"
#include "shared/util/UrlHelper.hxx"
#include "shared/validation/XmlValidator.hxx"

#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <tuple>


namespace
{
    std::string downloadFile (
        const UrlRequestSender& requestSender,
        const UrlHelper::UrlParts& url)
    {
        try
        {
            for (int i = 0; i < 3; ++i)
            {
                TVLOG(2) << "Trying to download file [" << url.toString() << "], try " << i;

                const std::string ciphers =
                    Configuration::instance().getStringValue(ConfigurationKey::TSL_DOWNLOAD_CIPHERS);
                const std::optional<std::string> forcedCiphers =
                    (ciphers.empty() ? std::nullopt : std::optional<std::string>{ciphers});
                const ClientResponse response = requestSender.send(
                    url,
                    HttpMethod::GET,
                    "",
                    {},
                    forcedCiphers,
                    true);

                if (response.getHeader().status() == HttpStatus::OK)
                {
                    TVLOG(2) << "Download succeeded";
                    return response.getBody();
                }
            }
        }
        catch(const std::runtime_error& e)
        {
            TLOG(ERROR) << "Can not Download [" << url.toString() << "], exception: " << e.what();
        }

        TslFail("Download failed for URL " + url.toString(), TslErrorCode::TSL_DOWNLOAD_ERROR);
    }

    bool checkC_HP_QES(const X509Certificate& certificate)
    {
        return certificate.checkCertificatePolicy(TslService::oid_hba_qes);
    }

    bool checkC_CH_QES(const X509Certificate& certificate)
    {
        return certificate.checkCertificatePolicy(TslService::oid_egk_qes);
    }

    bool checkC_HP_ENC(const X509Certificate& certificate)
    {
        return certificate.checkCertificatePolicy(TslService::oid_vk_eaa_enc);
    }

    bool checkC_CH_AUT(const X509Certificate& certificate)
    {
        return certificate.checkCertificatePolicy(TslService::oid_egk_aut);
    }

    bool checkC_CH_AUT_ALT(const X509Certificate& certificate)
    {
        return certificate.checkCertificatePolicy(TslService::oid_egk_aut_alt);
    }

    bool checkC_FD_AUT(const X509Certificate& certificate)
    {
        return certificate.checkCertificatePolicy(TslService::oid_fd_aut) &&
               certificate.checkRoles({TslService::oid_epa_vau, TslService::oid_erp_vau});
    }

    bool checkC_FD_TLS_S(const X509Certificate& certificate)
    {
        return certificate.checkCertificatePolicy(TslService::oid_fd_tls_s) &&
               certificate.checkRoles(
                   {std::string{profession_oid::oid_epa_dvw}, std::string{profession_oid::oid_epa_mgmt},
                    std::string{profession_oid::oid_epa_authn}, std::string{profession_oid::oid_epa_authz}});
    }

    std::vector<std::string> getTechnicalRolesOIDs()
    {
        return {
            std::string(profession_oid::oid_vsdd),
            std::string(profession_oid::oid_ocsp),
            std::string(profession_oid::oid_cms),
            std::string(profession_oid::oid_ufs),
            std::string(profession_oid::oid_ak),
            std::string(profession_oid::oid_nk),
            std::string(profession_oid::oid_kt),
            std::string(profession_oid::oid_sak),
            std::string(profession_oid::oid_int_vsdm),
            std::string(profession_oid::oid_konfigdienst),
            std::string(profession_oid::oid_vpnz_ti),
            std::string(profession_oid::oid_vpnz_sis),
            std::string(profession_oid::oid_cmfd),
            std::string(profession_oid::oid_vzd_ti),
            std::string(profession_oid::oid_komle),
            std::string(profession_oid::oid_komle_recipient_emails),
            std::string(profession_oid::oid_stamp),
            std::string(profession_oid::oid_tsl_ti),
            std::string(profession_oid::oid_wadg),
            std::string(profession_oid::oid_epa_authn),
            std::string(profession_oid::oid_epa_authz),
            std::string(profession_oid::oid_epa_dvw),
            std::string(profession_oid::oid_epa_mgmt),
            std::string(profession_oid::oid_epa_recovery),
            std::string(profession_oid::oid_epa_vau),
            std::string(profession_oid::oid_vz_tsp),
            std::string(profession_oid::oid_whk1_hsm),
            std::string(profession_oid::oid_whk2_hsm),
            std::string(profession_oid::oid_whk),
            std::string(profession_oid::oid_sgd1_hsm),
            std::string(profession_oid::oid_sgd2_hsm),
            std::string(profession_oid::oid_sgd),
            std::string(profession_oid::oid_erp_vau),
            std::string(profession_oid::oid_erezept),
            std::string(profession_oid::oid_idpd)
        };
    }

    bool checkC_FD_SIG(const X509Certificate& certificate)
    {
        return certificate.checkCertificatePolicy(TslService::oid_fd_sig)
               && certificate.checkRoles(getTechnicalRolesOIDs());
    }

    bool checkC_FD_OSIG(const X509Certificate& certificate)
    {
        return certificate.checkCertificatePolicy(TslService::oid_fd_osig) &&
               certificate.checkRoles(getTechnicalRolesOIDs());
    }

    bool checkC_HCI_ENC(const X509Certificate& certificate)
    {
        return certificate.checkCertificatePolicy(TslService::oid_smc_b_enc);
    }

    bool checkC_HCI_AUT(const X509Certificate& certificate)
    {
        return certificate.checkCertificatePolicy(TslService::oid_smc_b_aut);
    }

    bool checkC_HCI_OSIG(const X509Certificate& certificate)
    {
        return certificate.checkCertificatePolicy(TslService::oid_smc_b_osig);
    }

    bool checkC_ZD_TLS_S(const X509Certificate& certificate) // NOLINT(readability-identifier-naming)
    {
        return certificate.checkCertificatePolicy(TslService::oid_zd_tls_s);
    }

    bool isCertificateOfType(const X509Certificate& certificate, CertificateType certificateType)
    {
        switch (certificateType)
        {
            case CertificateType::C_CH_AUT:
                return checkC_CH_AUT(certificate);
            case CertificateType::C_CH_AUT_ALT:
                return checkC_CH_AUT_ALT(certificate);
            case CertificateType::C_FD_AUT:
                return checkC_FD_AUT(certificate);
            case CertificateType::C_FD_SIG:
                return checkC_FD_SIG(certificate);
            case CertificateType::C_FD_OSIG:
                return checkC_FD_OSIG(certificate);
            case CertificateType::C_FD_TLS_S:
                return checkC_FD_TLS_S(certificate);
            case CertificateType::C_HCI_ENC:
                return checkC_HCI_ENC(certificate);
            case CertificateType::C_HCI_AUT:
                return checkC_HCI_AUT(certificate);
            case CertificateType::C_HCI_OSIG:
                return checkC_HCI_OSIG(certificate);
            case CertificateType::C_HP_QES:
                return checkC_HP_QES(certificate);
            case CertificateType::C_CH_QES:
                return checkC_CH_QES(certificate);
            case CertificateType::C_HP_ENC:
                return checkC_HP_ENC(certificate);
            case CertificateType::C_ZD_TLS_S:
                return checkC_ZD_TLS_S(certificate);
        }
        return false;
    }


    // Data here taken from gemspec_PKI_V2.10.2.pdf
    std::vector<ExtendedKeyUsage> expectedExtendedKeyUsage(const CertificateType certificateType)
    {
        switch (certificateType)
        {
            case CertificateType::C_CH_AUT:
            case CertificateType::C_CH_AUT_ALT:
            case CertificateType::C_HCI_AUT:
                return {ExtendedKeyUsage::sslClient}; // id-kp-clientAuth
            case CertificateType::C_FD_TLS_S:
                return {ExtendedKeyUsage::sslServer};
            case CertificateType::C_FD_AUT:
            case CertificateType::C_FD_SIG:
            case CertificateType::C_FD_OSIG:
            case CertificateType::C_HCI_ENC:
            case CertificateType::C_HCI_OSIG:
            case CertificateType::C_HP_QES:
            case CertificateType::C_CH_QES:
            case CertificateType::C_HP_ENC:
                return std::vector<ExtendedKeyUsage>{};
            case CertificateType::C_ZD_TLS_S:
                return {ExtendedKeyUsage::sslServer};
        }

        TslFail("Unexpected certificate type", TslErrorCode::CERT_TYPE_MISMATCH);
    }


    // Data here taken from gemspec_PKI_V2.8.0.pdf
    std::vector<KeyUsage> expectedKeyUsage(const X509Certificate& certificate,
                                           const CertificateType certificateType)
    {
        switch (certificateType)
        {
            case CertificateType::C_CH_AUT: // NOLINT(bugprone-branch-clone)
                //GS_A_4595 begin - expected key usage
                // in case of RSA certificate keyEncipherment is optional
                // our implementation checks only required key usages
                return {KeyUsage::digitalSignature};
                //GS_A_4595 end
            case CertificateType::C_CH_AUT_ALT:
                //A_17989 begin - expected key usage
                // in case of RSA certificate keyEncipherment is optional
                // our implementation checks only required key usages
                return {KeyUsage::digitalSignature};
                //A_17989 end
            case CertificateType::C_HCI_AUT:
            case CertificateType::C_FD_AUT:
            case CertificateType::C_ZD_TLS_S:
                if (certificate.getSigningAlgorithm() == SigningAlgorithm::ellipticCurve)
                {
                    return {KeyUsage::digitalSignature};
                }
                else
                {
                    return {KeyUsage::digitalSignature, KeyUsage::keyEncipherment};
                }
            case CertificateType::C_FD_SIG:
            case CertificateType::C_FD_TLS_S:
                return {KeyUsage::digitalSignature};

            case CertificateType::C_HCI_ENC:
                if (certificate.getSigningAlgorithm() == SigningAlgorithm::ellipticCurve)
                {
                    return {KeyUsage::keyAgreement};
                }
                else
                {
                    return {KeyUsage::dataEncipherment, KeyUsage::keyEncipherment};
                }
            case CertificateType::C_FD_OSIG:
            case CertificateType::C_HCI_OSIG:
            case CertificateType::C_HP_QES:
            case CertificateType::C_CH_QES:
            case CertificateType::C_HP_ENC:
                return {KeyUsage::nonRepudiation};
        }

        TslFail("Unexpected certificate type", TslErrorCode::CERT_TYPE_MISMATCH);
    }

    bool isQesCertificate(const CertificateType certificateType)
    {
        return certificateType == CertificateType::C_HP_QES
               || certificateType == CertificateType::C_CH_QES
               || certificateType == CertificateType::C_HP_ENC;
    }

    bool hashExtensionMustBeValidated(const CertificateType certificateType)
    {
        return isQesCertificate(certificateType)
               || (certificateType != CertificateType::C_CH_AUT
                   && certificateType != CertificateType::C_CH_AUT_ALT);
    }

    std::string to_string(const CertificateType certificateType)
    {
        switch(certificateType)
        {
            case CertificateType::C_CH_AUT:
                return "C_CH_AUT";
            case CertificateType::C_CH_AUT_ALT:
                return "C_CH_AUT_ALT";
            case CertificateType::C_FD_AUT:
                return "C_FD_AUT";
            case CertificateType::C_FD_SIG:
                return "C_FD_SIG";
            case CertificateType::C_FD_OSIG:
                return "C_FD_OSIG";
            case CertificateType::C_FD_TLS_S:
                return "C_FD_TLS_S";
            case CertificateType::C_HCI_ENC:
                return "C_HCI_ENC";
            case CertificateType::C_HCI_AUT:
                return "C_HCI_AUT";
            case CertificateType::C_HCI_OSIG:
                return "C_HCI_OSIG";
            case CertificateType::C_HP_QES:
                return "C_HP_QES";
            case CertificateType::C_CH_QES:
                return "C_CH_QES";
            case CertificateType::C_HP_ENC:
                return "C_HP_ENC";
            case CertificateType::C_ZD_TLS_S:
                return "C_ZD_TLS_S";
        }

        TLOG(ERROR) << "CertificateType enum was extended, but this implementation was not";
        return "unknown";
    }


    std::string getCertificateTypeOid(const CertificateType certificateType)
    {
        switch(certificateType)
        {
            case CertificateType::C_CH_AUT:
                return TslService::oid_egk_aut;
            case CertificateType::C_CH_AUT_ALT:
                return TslService::oid_egk_aut_alt;
            case CertificateType::C_FD_AUT:
                return TslService::oid_fd_aut;
            case CertificateType::C_FD_SIG:
                return TslService::oid_fd_sig;
            case CertificateType::C_FD_OSIG:
                return TslService::oid_fd_osig;
            case CertificateType::C_FD_TLS_S:
                return TslService::oid_fd_tls_s;
            case CertificateType::C_HCI_ENC:
                return TslService::oid_smc_b_enc;
            case CertificateType::C_HCI_AUT:
                return TslService::oid_smc_b_aut;
            case CertificateType::C_HCI_OSIG:
                return TslService::oid_smc_b_osig;
            case CertificateType::C_HP_QES:
                return TslService::oid_hba_qes;
            case CertificateType::C_CH_QES:
                return TslService::oid_egk_qes;
            case CertificateType::C_HP_ENC:
                return TslService::oid_vk_eaa_enc;
            case CertificateType::C_ZD_TLS_S:
                return TslService::oid_zd_tls_s;
        }

        TLOG(ERROR) << "CertificateType enum was extended, but this implementation was not";
        return "unknown";
    }


    std::unordered_set<int> getSupportedCriticalExtensionsNids(const CertificateType certificateType)
    {
        switch(certificateType)
        {
            case CertificateType::C_CH_AUT:
            case CertificateType::C_CH_AUT_ALT:
            case CertificateType::C_FD_AUT:
            case CertificateType::C_FD_SIG:
            case CertificateType::C_FD_OSIG:
            case CertificateType::C_FD_TLS_S:
            case CertificateType::C_HCI_ENC:
            case CertificateType::C_HCI_AUT:
            case CertificateType::C_HCI_OSIG:
            case CertificateType::C_HP_QES:
            case CertificateType::C_CH_QES:
            case CertificateType::C_HP_ENC:
            case CertificateType::C_ZD_TLS_S:
                return {NID_key_usage, NID_basic_constraints};
        }

        TslFail("Unexpected certificate type", TslErrorCode::CERT_TYPE_MISMATCH);
    }


    bool tslAcceptsType(
        const CertificateType certificateType,
        const TslParser::ExtensionOidList& extensionOidList,
        const TslMode tslMode)
    {
        if (tslMode == TslMode::TSL)
        {
            return (extensionOidList.find(getCertificateTypeOid(certificateType)) != extensionOidList.end());
        }

        return true;
    }


    X509Certificate getIssuerCertificate(const X509Certificate& certificate,
                                         const TslParser::ServiceInformationMap& serviceInformationMap)
    {
        auto it = serviceInformationMap.find({certificate.getIssuer(), certificate.getAuthorityKeyIdentifier()});
        if (it != serviceInformationMap.end()
            && certificate.getAuthorityKeyIdentifier() == it->second.certificate.getSubjectKeyIdentifier())
        {
            return it->second.certificate;
        }

        return {};
    }

    std::optional<OcspUrl> primaryOcspServiceUrlForCertificate(
        const X509Certificate& certificate,
        const TslParser::ServiceInformationMap& serviceInformationMap)
    {
        const auto iterator = serviceInformationMap.find(
            {certificate.getSubject(), certificate.getSubjectKeyIdentifier()});
        if (iterator != serviceInformationMap.cend() && !iterator->second.serviceSupplyPointList.empty())
        {
            return OcspUrl{iterator->second.serviceSupplyPointList.front(), false};
        }

        return std::nullopt;
    }


    void validateOcspStatusOfSignerCertificate(
        std::optional<TslParser>& tslParser,
        const UrlRequestSender& requestSender,
        TrustStore& trustStore)
    {
        // check OCSP status of signer certificate
        X509Certificate issuerCertificate =
            getIssuerCertificate(tslParser->getSignerCertificate(),
                                 tslParser->getServiceInformationMap());
        TslExpect6(issuerCertificate.hasX509Ptr(),
                  "issuer certificate must be part of TSL",
                  TslErrorCode::CA_CERT_MISSING,
                   trustStore.getTslMode(),
                   trustStore.getIdOfTslInUse(),
                   trustStore.getSequenceNumberOfTslInUse());

        const std::optional<OcspUrl> ocspUrl =
            primaryOcspServiceUrlForCertificate(issuerCertificate, tslParser->getServiceInformationMap());
        TslExpect6(ocspUrl.has_value(),
                  "ocsp URL must be provided für TSL signer certificate",
                  TslErrorCode::SERVICESUPPLYPOINT_MISSING,
                   trustStore.getTslMode(),
                   trustStore.getIdOfTslInUse(),
                   trustStore.getSequenceNumberOfTslInUse());

        const auto ocspStatus = OcspService::getCurrentStatusOfTslSigner(
            tslParser->getSignerCertificate(),
            issuerCertificate,
            requestSender,
            *ocspUrl,
            trustStore, // old trust store is provided here for cached data
            trustStore.hasTsl() ? std::nullopt
                                : std::make_optional(tslParser->getOcspCertificateList()),
            true);

        TslExpect6(ocspStatus.certificateStatus == CertificateStatus::good,
                   std::string{"OCSP check for TSL signer certificate has failed, oscp certificate status: "}
                       .append(magic_enum::enum_name(ocspStatus.certificateStatus))
                       .append(", certificate subject: ")
                       .append(issuerCertificate.getSubject())
                       .append(", serial: ")
                       .append(issuerCertificate.getSerialNumber())
                       .append(", ocspUrl: ")
                       .append(ocspUrl->url),
                   TslErrorCode::OCSP_STATUS_ERROR, trustStore.getTslMode(), trustStore.getIdOfTslInUse(),
                   trustStore.getSequenceNumberOfTslInUse());
    }


    void refreshTrustStore(std::optional<TslParser>& tslParser,
                           TrustStore& trustStore)
    {
        TVLOG(2) << "TrustServiceStatusList downloaded and refresh of TrustStore is needed";

        try
        {
            Expect(tslParser.has_value(), "the parser must be provided");

            trustStore.refillFromTsl(*tslParser);
            TVLOG(2) << "TrustStore refreshed with downloaded TrustServiceStatusList";
        }
        catch(const std::runtime_error&)
        {
            if (trustStore.isTslTooOld())
            {
                GS_A_5336.start("Remove certificates from outdated trust store.");
                trustStore.distrustCertificates();
                GS_A_5336.finish();
            }

            throw;
        }
    }


    std::string downloadNewHashValue (
        const UrlRequestSender& requestSender, const std::string& tslUrl)
    {
        UrlHelper::UrlParts urlParts = UrlHelper::parseUrl(tslUrl);
        const std::string expectedExtension = ".xml";
        if (String::ends_with(urlParts.mPath, expectedExtension))
        {
            std::string hashPath =
                boost::algorithm::replace_last_copy(urlParts.mPath, expectedExtension, ".sha2");

            // There is an inconsistency in Gematik Specification regarding downloading of TSL per http protocol
            //
            // gemSpec_PKI
            // TUC_PKI_019 „Prüfung der Aktualität der TSL":
            //     [System:] Wenn ein TSL-Hashwert als Eingangsparameter im System vorhanden ist, wird die aktuelle
            //     Hashwert-Datei der TSL vom TSL Downloadpunkt heruntergeladen. Dazu wird der TSL- Downloadpunkt
            //     ermittelt (TUC_PKI_017 „Lokalisierung TSL Download-Adressen”) und von der emittelten URI statt
            //     der Datei mit Endung „*.xml” die Datei mit Endung „*.sha2" heruntergeladen.
            //
            // gemSpec_TSL
            // specifies existence of .sha2 endpoints only for https-protocols
            // for example in A_17682 or in Productive Environment description.
            //
            // Workaround is to try to download .sha file using https protocol always.
            if (String::toLower(urlParts.mProtocol) == UrlHelper::HTTPS_PROTOCOL)
            {
                return downloadFile(
                    requestSender,
                    { urlParts.mProtocol,
                      urlParts.mHost,
                      urlParts.mPort,
                      hashPath,
                      urlParts.mRest});
            }
            else
            {
                return downloadFile(
                    requestSender,
                    { UrlHelper::HTTPS_PROTOCOL,
                      urlParts.mHost,
                      443,
                      hashPath,
                      urlParts.mRest});
            }
        }

        TslFail("Unexpected TrustServiceStatusList link format, can not generate signature link, original URL: " + tslUrl,
                       TslErrorCode::TSL_DOWNLOAD_ERROR);
    }


    std::optional<std::string> getTslHashValue(
        const UrlRequestSender& requestSender,
        TrustStore& trustStore)
    {
        for (const std::string& updateUrlString : trustStore.getUpdateUrls())
        {
            try
            {
                auto newHashValue = String::toLower(String::trim(downloadNewHashValue(requestSender, updateUrlString)));
                if (! newHashValue.empty())
                {
                    if (!String::isHexString(newHashValue))
                    {
                        LOG(WARNING) << "Downloaded hash value of TrustServiceStatusList is not a valid hex string";
                        newHashValue = String::toHexString(newHashValue);
                    }
                    return newHashValue;
                }
            }
            catch (const TslError&)
            {
                TLOG(ERROR) << "Can not access " << updateUrlString;
            }
        }

        TslFail("Can not download hash value of TrustServiceStatusList.", TslErrorCode::TSL_DOWNLOAD_ERROR);
    }


    bool tslNeedsRefresh(
        const std::optional<std::string>& newHash,
        TrustStore& trustStore)
    {
        bool needsUpdate = !trustStore.hasTsl();

        if (! needsUpdate)
        {
            needsUpdate = trustStore.isTslTooOld();
            if (! needsUpdate)
            {
                TslExpect(newHash.has_value(), "Hash value missing", TslErrorCode::TSL_DOWNLOAD_ERROR);
                // if the new version of TSL.xml is available according to hash check
                // or if the current version is outdated ( and hash is not available )
                // the TSL should be refreshed
                needsUpdate = (newHash.has_value() && newHash != trustStore.getTslHashValue());
            }
        }

        return needsUpdate;
    }


    OcspUrl getOcspUrl(const X509Certificate& certificate,
                           const CertificateType certificateType,
                           const X509Certificate& issueCertificate,
                           TrustStore& trustStore)
    {
        OcspUrl ocspUrl;

        if (isQesCertificate(certificateType))
        {
            const std::vector<std::string> ocspUrls = certificate.getOcspUrls();
            TslExpect(ocspUrls.size() == 1,
                      "OCSP check failed, TUC_PKI_030 expects exactly one OCSP addresse.",
                      TslErrorCode::TSL_NOT_WELLFORMED);
            const auto ocspMapping = trustStore.getBnaOcspMapping();
            const auto iterator = ocspMapping.find(ocspUrls[0]);
            if (iterator != ocspMapping.end())
            {
                ocspUrl = {iterator->second, false};
            }
            else
            {
                // Special handling for G0 QES certificates for which no mapping exists in the TSL
                // and in this case a special TI Ocsp proxy should be used
                // The Gematik TI proxy works in the way that the original ocsp url is appended to the
                // the proxy url, e.g. "http://ocsp.proxy.ibm.de/http://ocsp.test.ibm.de/"
                std::string tiOcspProxyUrl =
                    Configuration::instance().getStringValue(ConfigurationKey::TSL_TI_OCSP_PROXY_URL);
                if (!tiOcspProxyUrl.empty())
                {
                    if (tiOcspProxyUrl.back() != '/')
                        tiOcspProxyUrl.append("/");

                    ocspUrl = {tiOcspProxyUrl + ocspUrls[0], false};
                }
                else
                {
                    ocspUrl = {ocspUrls[0], true};
                }
            }
        }
        else
        {
            const std::optional<std::string> ocspUrlOptional =
                trustStore.primaryOcspServiceUrlForCertificate(issueCertificate);
            TslExpect6(ocspUrlOptional.has_value(),
                       "OCSP check failed, because no supply point has been found in trust store.",
                       TslErrorCode::SERVICESUPPLYPOINT_MISSING,
                       trustStore.getTslMode(),
                       trustStore.getIdOfTslInUse(),
                       trustStore.getSequenceNumberOfTslInUse());
            ocspUrl = {*ocspUrlOptional, false};
        }

        return ocspUrl;
    }


    // GEMREQ-start A_22141#checkOcspStatusOfCertificate, A_20159-03#checkOcspStatusOfCertificate
    OcspResponse checkOcspStatusOfCertificate(const X509Certificate& certificate,
                                      const CertificateType certificateType,
                                      const X509Certificate& issueCertificate,
                                      const UrlRequestSender& requestSender,
                                      TrustStore& trustStore,
                                      const OcspCheckDescriptor& ocspCheckDescriptor)
    {
        const auto ocspUrl = getOcspUrl(certificate, certificateType, issueCertificate, trustStore);

        TVLOG(2) << "Performing ocsp check with url " << ocspUrl.url << ".";

        auto ocspResponse =
            OcspService::getCurrentResponse(certificate, requestSender, ocspUrl, trustStore,
                                            hashExtensionMustBeValidated(certificateType), ocspCheckDescriptor);
        ocspResponse.checkStatus(trustStore, ocspCheckDescriptor. timeSettings.referenceTimePoint);
        return ocspResponse;
    }
    // GEMREQ-end A_22141#checkOcspStatusOfCertificate, A_20159-03#checkOcspStatusOfCertificate

    void checkCertificateChain(const X509Certificate& certificate,
                               TrustStore& trustStore)
    {
        try
        {
            auto storeContext = std::unique_ptr<X509_STORE_CTX, void(*)(X509_STORE_CTX*)>(
                X509_STORE_CTX_new(), X509_STORE_CTX_free);
            OpenSslExpect(storeContext != nullptr, "Can not create store context.");

            const auto* x509Certificate = certificate.getX509ConstPtr();
            X509Store x509Store = trustStore.getX509Store(certificate);
            OpenSslExpect(1 == X509_STORE_CTX_init(storeContext.get(),
                                                   x509Store.getStore(),
                                                   const_cast<X509*>(x509Certificate), // NOLINT(cppcoreguidelines-pro-type-const-cast)
                                                   nullptr),
                          "Can not init store context");
            X509_VERIFY_PARAM *param = X509_STORE_CTX_get0_param(storeContext.get());
            X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_PARTIAL_CHAIN);

            if (trustStore.getTslMode() == TslMode::BNA)
            {
                auto* notBefore = X509_get_notBefore(x509Certificate);
                OpenSslExpect(notBefore != nullptr, "Can not get notBefore info.");
                OpenSslExpect(1 == ASN1_TIME_normalize(notBefore), "Can not normalize notBefore ASN1 time.");

                tm notBeforeTm{};
                OpenSslExpect(1 == ASN1_TIME_to_tm(notBefore, &notBeforeTm), "Can not convert ASN1 time to tm.");

                const auto timestamp = model::Timestamp::fromTmInUtc(notBeforeTm);
                X509_VERIFY_PARAM_set_time(param, timestamp.toTimeT());
            }

            OpenSslExpect(X509_verify_cert(storeContext.get()) == 1, "Signer certificate is invalid");

            STACK_OF(X509)* verifiedChain = X509_STORE_CTX_get0_chain(storeContext.get());
            int chainLength = sk_X509_num(verifiedChain);
            OpenSslExpect(chainLength > 0,
                          "Verified chain by certificate must not be empty.");
        }
        catch(const std::runtime_error& e)
        {
            TslFail5(std::string("Certificate chain verification failed: ") + e.what(),
                     TslErrorCode::CERTIFICATE_NOT_VALID_MATH,
                     trustStore.getTslMode(),
                     trustStore.getIdOfTslInUse(),
                     trustStore.getSequenceNumberOfTslInUse());
        }
    }


    void checkQcStatement(const X509Certificate& certificate, const CertificateType certificateType)
    {
        if (isQesCertificate(certificateType))
        {
            TslExpect(certificate.checkQcStatement(TslService::id_etsi_qcs_QcCompliance),
                      "No expected for QES certificate QC-Statement found.",
                      TslErrorCode::QC_STATEMENT_ERROR);
        }
    }


    void checkTrustStoreAfterUpdate(TrustStore& trustStore)
    {
        if (trustStore.isTslTooOld())
        {
            GS_A_5336.start("Remove certificates from outdated or invalid trust store.");
            trustStore.distrustCertificates();
            GS_A_5336.finish();

            TslFail5("TSL file is outdated and no update is possible.",
                     trustStore.getTslMode() == TslMode::TSL
                     ? TslErrorCode::VALIDITY_WARNING_2
                     : TslErrorCode::VL_UPDATE_ERROR,
                     trustStore.getTslMode(),
                     trustStore.getIdOfTslInUse(),
                     trustStore.getSequenceNumberOfTslInUse());
        }
    }


    void checkTrustStoreAfterUpdateError(TrustStore& trustStore, const TslError& tslError)
    {
        if ( ! trustStore.hasTsl())
        {
            TslFailStack("TSL file is outdated and no update is possible.",
                         trustStore.getTslMode() == TslMode::TSL
                         ? TslErrorCode::TSL_INIT_ERROR
                         : TslErrorCode::VL_UPDATE_ERROR,
                         tslError);
        }

        if (trustStore.isTslTooOld())
        {
            GS_A_5336.start("Remove certificates from outdated or invalid trust store.");
            trustStore.distrustCertificates();
            GS_A_5336.finish();

            TslFailStack("TSL file is outdated and no update is possible.",
                         trustStore.getTslMode() == TslMode::TSL
                         ? TslErrorCode::VALIDITY_WARNING_2
                         : TslErrorCode::VL_UPDATE_ERROR,
                         tslError);
        }
    }


    void checkCriticalExtensions(const X509Certificate& certificate, const CertificateType certificateType)
    {
        GS_A_4661_01.start("Verify critical extensions.");
        TslExpect(certificate.checkCritical(getSupportedCriticalExtensionsNids(certificateType)),
                  "Unexpected critical extension in certificate.",
                  TslErrorCode::CERT_TYPE_MISMATCH);
        TslExpect( ! certificate.isCaCert(),
                   "The certificate must not be CA-certificate",
                   TslErrorCode::CERT_TYPE_MISMATCH);
        TslExpect(certificate.checkKeyUsage(expectedKeyUsage(certificate, certificateType)),
                  "Invalid key usage.",
                  TslErrorCode::WRONG_KEYUSAGE);
        GS_A_4661_01.finish();
    }
} // anonymous namespace


// gemSpec_PKI § 8.2.2.3 TUC_PKI_011: Prüfung des TSL-Signer-Zertifikates
// GEMREQ-start GS-A_4650
void
TslService::checkSignerCertificate (const X509Certificate& signerCertificate,
                                    TrustStore& trustStore,
                                    const std::vector<X509Certificate>& expectedSignerCertificates)
{
    // gemSpec_PKI § 8.3.1.2 TUC_PKI_002: Gültigkeitsprüfung des Zertifikates
    // GEMREQ-start GS-A_4653
    TslExpect(signerCertificate.checkValidityPeriod(),
              "TSL signer certificate is outdated.",
              TslErrorCode::CERTIFICATE_NOT_VALID_TIME);
    // GEMREQ-end GS-A_4653

    TslExpect4(signerCertificate.checkKeyUsage({KeyUsage::nonRepudiation}),
               "Wrong key usage in TrustServiceStatusList signer certificate",
               TslErrorCode::WRONG_KEYUSAGE,
               trustStore.getTslMode());

    checkTslSignerExtendedKeyUsage(signerCertificate);

    // if already accepted expected signed certificates are not provided,
    // check the signature of the signer certificate
    if (expectedSignerCertificates.empty())
    {
        const std::vector<X509Certificate> tslSignerCas = trustStore.getTslSignerCas();

        for (const X509Certificate& tslSignerCa : tslSignerCas)
        {
            TslExpect(tslSignerCa.hasX509Ptr(),
                      "TSL signer certificate CA is not loaded",
                      TslErrorCode::TSL_CA_NOT_LOADED);
        }

        // gemSpec_PKI § 8.3.1.4 TUC_PKI_004: Mathematische Prüfung der Zertifikatssignatur
        // GEMREQ-start GS-A_4655
        for (const X509Certificate& tslSignerCa : tslSignerCas)
        {
            // Accept this signer certificate as valid if it was signed by _any_ of the currently valid
            // signer CA certificates.
            if (signerCertificate.signatureIsValidAndWasSignedBy(tslSignerCa))
                return;
        }

        TslExpect(std::ranges::find(expectedSignerCertificates, signerCertificate) !=
                  expectedSignerCertificates.end(),
                  "TSL signer certificate is not signed with expected CA",
                  TslErrorCode::CERTIFICATE_NOT_VALID_MATH);
        // GEMREQ-end GS-A_4655
    }
    else
    {
        // if already accepted expected signed certificates are provided,
        // the signer certificate must be in the list
        TslExpect(std::ranges::find(expectedSignerCertificates, signerCertificate) !=
                      expectedSignerCertificates.end(),
                  "Unexpected signer certificate.",
                  TslErrorCode::CERTIFICATE_NOT_VALID_MATH);
    }
}
// GEMREQ-end GS-A_4650


void TslService::checkTslSignerExtendedKeyUsage(const X509Certificate& certificate)
{
    int criticality = 0, extIndex = -1;
    std::shared_ptr<EXTENDED_KEY_USAGE> extendedKeyUsage{
            static_cast<EXTENDED_KEY_USAGE*>(X509_get_ext_d2i(
            certificate.getX509ConstPtr(),
                                                              NID_ext_key_usage,
                                                              &criticality, &extIndex)),
            EXTENDED_KEY_USAGE_free};
    TslExpect(criticality != -1 && extendedKeyUsage,
              "Unexpected extended key usage",
              TslErrorCode::WRONG_EXTENDEDKEYUSAGE);

    const int numberOfObjects{sk_ASN1_OBJECT_num(extendedKeyUsage.get())};
    constexpr const char* id_tsl_kp_tslSigning{"0.4.0.2231.3.0"};
    std::shared_ptr<ASN1_OBJECT> objectToSearchFor{OBJ_txt2obj(id_tsl_kp_tslSigning, 1),
                                                   ASN1_OBJECT_free};
    for (int i = 0; i < numberOfObjects; ++i)
    {
        ASN1_OBJECT* asnObject = sk_ASN1_OBJECT_value(extendedKeyUsage.get(), i);
        if (OBJ_cmp(asnObject, objectToSearchFor.get()) == 0)
        {
            return;
        }
    }

    TslFail("Missing id_tsl_kp_tslSigning in extended key usage",
            TslErrorCode::WRONG_EXTENDEDKEYUSAGE);
}


// gemSpec_PKI § 8.2.2.1 TUC_PKI_019: Prüfung der Aktualität der TSL
// GEMREQ-start GS-A_4648
std::optional<TslParser>
TslService::refreshTslIfNecessary(
    UrlRequestSender& requestSender,
    const XmlValidator& xmlValidator,
    TrustStore& trustStore,
    const std::optional<std::string>& newHash,
    const std::vector<X509Certificate>& expectedSignerCertificates)
{
    // 1. Download TSL (TUC_PKI_016)
    if (tslNeedsRefresh(newHash, trustStore))
    {
        std::string tslContent;
        const auto updateUrls = trustStore.getUpdateUrls();
        for (size_t ind = 0; ind < updateUrls.size() && tslContent.empty(); ind++)
        {
            try
            {
                const UrlHelper::UrlParts updateUrl = UrlHelper::parseUrl(updateUrls[ind]);
                tslContent = downloadFile(requestSender, updateUrl);
            }
            catch (const TslError&)
            {
                TLOG(ERROR) << "Can not access " << updateUrls[ind];
            }
        }

        TslExpect(! tslContent.empty(),
                  "Can not download new TrustServiceStatusList version. Hash: " + newHash.value_or(""),
                  TslErrorCode::TSL_DOWNLOAD_ERROR);
        TLOG(INFO) << "Successfully downloaded " << magic_enum::enum_name(trustStore.getTslMode())
                   << " with hash " + newHash.value_or("");
        const auto contentHash = String::toLower(String::toHexString(Hash::sha256(tslContent)));
        TVLOG(1) << "Downloaded TSL calculated hash: " << contentHash;
        if (newHash.has_value() && contentHash != newHash)
        {
            TLOG(WARNING) << "Downloaded " << magic_enum::enum_name(trustStore.getTslMode())
                          << " has wrong hash. Expected: " << newHash.value_or("") << ", calculated: " << contentHash;
        }

        return attemptTslParsing(tslContent, xmlValidator, trustStore, expectedSignerCertificates);
    }

    Expect( ! trustStore.isTslTooOld(), "Unexpected trust store state.");
    return std::nullopt;
}
// GEMREQ-end GS-A_4648


std::optional<TslParser>
TslService::attemptTslParsing (const std::string& tslXml,
                               const XmlValidator& xmlValidator,
                               TrustStore& trustStore,
                               const std::vector<X509Certificate>& expectedSignerCertificates = {})
{
    TslParser tslParser{tslXml, trustStore.getTslMode(), xmlValidator};

    checkSignerCertificate(tslParser.getSignerCertificate(),
                           trustStore,
                           expectedSignerCertificates);

    // 5. Select TSLSequenceNumber, both for the new TSL and for the
    // TSL used by the system
    const auto newTslSequenceNumber = std::stoll(tslParser.getSequenceNumber());
    const auto systemTslSequenceNumber = !trustStore.getSequenceNumberOfTslInUse().empty()
                                                   ? std::stoll(trustStore.
                                                                  getSequenceNumberOfTslInUse())
                                                   : 0;
    if (trustStore.getTslMode() == TslMode::TSL)
    {
        // in the case for Gematik-TSL obtain and validate the id
        const auto& newId = tslParser.getId();
        const auto& systemId = trustStore.getIdOfTslInUse();
        if (newId.has_value() && systemId != newId && systemTslSequenceNumber < newTslSequenceNumber)
        {
            return tslParser;
        }

        TslExpect6(newId.has_value() && systemId == newId && systemTslSequenceNumber == newTslSequenceNumber,
                   "Unexpected TrustServiceStatusList ID, new Id: " + newId.value_or("<unset>") +
                       ", newSequenceNumber: " + tslParser.getSequenceNumber(),
                   TslErrorCode::TSL_ID_INCORRECT, trustStore.getTslMode(), trustStore.getIdOfTslInUse(),
                   trustStore.getSequenceNumberOfTslInUse());
    }
    else if (systemTslSequenceNumber < newTslSequenceNumber)
    {
        return tslParser;
    }

    return std::nullopt;
}


// GEMREQ-start A_15874
TslService::UpdateResult TslService::triggerTslUpdateIfNecessary(
    UrlRequestSender& requestSender,
    const XmlValidator& xmlValidator,
    TrustStore& trustStore,
    const bool onlyOutdated,
    const std::vector<X509Certificate>& expectedSignerCertificates)
{
    auto updateResult = UpdateResult::NotUpdated;
    static std::mutex trustStoreUpdateMutex{};

    // technically speaking this mutex can be locked in more specific critical areas,
    // but since the overhead is not very huge we can lock it at the beginning of the method
    //
    std::lock_guard lock{trustStoreUpdateMutex};

    if (!trustStore.hasTsl() ||
        trustStore.isTslTooOld() ||
        !onlyOutdated)
    {
        try
        {
            std::optional<std::string> hash;
            if (trustStore.hasTsl())
            {
                // only download a new hash value if we already have downloaded it
                // once, otherwise we do not have a root store to validate the HTTPS
                // connection against
                hash = getTslHashValue(requestSender, trustStore);
            }
            auto returnedTslParser = refreshTslIfNecessary(
                requestSender, xmlValidator, trustStore, hash, expectedSignerCertificates);

            // GEMREQ-start A_16489
            if (returnedTslParser.has_value())
            {
                if (trustStore.getTslMode() == TslMode::TSL)
                {
                    validateOcspStatusOfSignerCertificate(
                        returnedTslParser,
                        requestSender,
                        trustStore); // old trust store is provided here for cached data
                }

                refreshTrustStore(returnedTslParser, trustStore);
                trustStore.setTslHashValue(returnedTslParser->getSha256());
                updateResult = UpdateResult::Updated;
            }
        }
        catch(const TslError& e)
        {
            checkTrustStoreAfterUpdateError(trustStore, e);
            TLOG(ERROR) << "TSL update has failed with TslError";
            TLOG(ERROR) << e.what();
        }

        // GEMREQ-start A_16490
        checkTrustStoreAfterUpdate(trustStore);
        // GEMREQ-end A_16490
        // GEMREQ-end A_16489
    }

    return updateResult;
}
// GEMREQ-end A_15874


CertificateType TslService::getCertificateType(const X509Certificate& certificate)
{
    TslExpect(certificate.hasCertificatePolicy(),
              "All supported certificate types must have policy set",
              TslErrorCode::CERT_TYPE_INFO_MISSING);

    for (const CertificateType certificateType : magic_enum::enum_values<CertificateType>())
    {
        if (isCertificateOfType(certificate, certificateType))
        {
            return certificateType;
        }
    }

    TslFail("Unexpected certificate type", TslErrorCode::CERT_TYPE_MISMATCH);
}

// GEMREQ-start A_22141#checkCertificateWithoutOcspCheck, A_20159-03#checkCertificateWithoutOcspCheck
std::tuple<CertificateType, X509Certificate>
TslService::checkCertificateWithoutOcspCheck(
    const X509Certificate& certificate,
    const std::unordered_set<CertificateType>& typeRestrictions,
    TrustStore& trustStore)
{
    TVLOG(2) << "Checking Certificate: [" << certificate.toBase64() << "]";
    const CertificateType certificateType = getCertificateType(certificate);
    TVLOG(2) << "Certificate of type " << to_string(certificateType) << " is being checked.";
    TslExpect(typeRestrictions.empty() || typeRestrictions.find(certificateType) != typeRestrictions.end(),
              "Certificate of unexpected type " + to_string(certificateType) + " provided.",
              TslErrorCode::CERT_TYPE_MISMATCH);
    Expect(trustStore.getTslMode() != TslMode::BNA || isQesCertificate(certificateType),
           "Only QES certificates are verified using BNA-TrustStore.");

    TVLOG(2) << "Checking certificate (" << certificate.getSubject() << ")..";

    checkQcStatement(certificate, certificateType);

    checkCriticalExtensions(certificate, certificateType);

    TslExpect6(certificate.checkValidityPeriod(),
               "The certificate must be valid.",
               TslErrorCode::CERTIFICATE_NOT_VALID_TIME,
               trustStore.getTslMode(),
               trustStore.getIdOfTslInUse(),
               trustStore.getSequenceNumberOfTslInUse());

    const auto caInfo = trustStore.lookupCaCertificate(certificate);
    if (!caInfo.has_value() || !caInfo->certificate.hasX509Ptr())
    {
        TslExpect6(trustStore.hasCaCertificateWithSubject(certificate.getIssuer()),
                   "Issuer is unknown.",
                   TslErrorCode::CA_CERT_MISSING,
                   trustStore.getTslMode(),
                   trustStore.getIdOfTslInUse(),
                   trustStore.getSequenceNumberOfTslInUse());

        TslFail5("Issuer has known subjectDN but subjectKeyIdentifier is unknown.",
                 TslErrorCode::AUTHORITYKEYID_DIFFERENT,
                 trustStore.getTslMode(),
                 trustStore.getIdOfTslInUse(),
                 trustStore.getSequenceNumberOfTslInUse());
    }

    TslExpect6(caInfo->accepted,
               "Issuer is revoked.",
               trustStore.getTslMode() == TslMode::TSL
                   ? TslErrorCode::CA_CERTIFICATE_REVOKED_IN_TSL
                   : TslErrorCode::CA_CERTIFICATE_REVOKED_IN_BNETZA_VL,
               trustStore.getTslMode(),
               trustStore.getIdOfTslInUse(),
               trustStore.getSequenceNumberOfTslInUse());

    TslExpect6(tslAcceptsType(certificateType, caInfo->extensionOidList, trustStore.getTslMode()),
               "TSL CA does not accept the certificate type.",
               TslErrorCode::CERT_TYPE_CA_NOT_AUTHORIZED,
               trustStore.getTslMode(),
               trustStore.getIdOfTslInUse(),
               trustStore.getSequenceNumberOfTslInUse());

    checkCertificateChain(certificate, trustStore);

    TslExpect(certificate.checkExtendedKeyUsage(expectedExtendedKeyUsage(certificateType)),
              "Invalid extended key usage.",
              TslErrorCode::WRONG_EXTENDEDKEYUSAGE);

    return {certificateType, caInfo->certificate};
}
// GEMREQ-end A_22141#checkCertificateWithoutOcspCheck, A_20159-03#checkCertificateWithoutOcspCheck


// GEMREQ-start A_22141#checkCertificate, A_20159-03#checkCertificate
OcspResponse
TslService::checkCertificate(
    const X509Certificate& certificate,
    const std::unordered_set<CertificateType>& typeRestrictions,
    const UrlRequestSender& requestSender,
    TrustStore& trustStore,
    const OcspCheckDescriptor& ocspCheckDescriptor)
{
    CertificateType certificateType; // NOLINT(cppcoreguidelines-init-variables)
    X509Certificate issuerCertificate;
    try
    {
        std::tie(certificateType, issuerCertificate) =
            checkCertificateWithoutOcspCheck(certificate, typeRestrictions, trustStore);
    }
    catch(const std::exception&)
    {
        // if the certificate is no more valid the cached OCSP response data should be removed
        trustStore.cleanCachedOcspData(certificate.getSha256FingerprintHex());
        throw;
    }

    return checkOcspStatusOfCertificate(
        certificate,
        certificateType,
        issuerCertificate,
        requestSender,
        trustStore,
        ocspCheckDescriptor);
}
// GEMREQ-end A_22141#checkCertificate, A_20159-03#checkCertificate

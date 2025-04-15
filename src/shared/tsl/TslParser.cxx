/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/tsl/TslParser.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/tsl/C14NHelper.hxx"
#include "shared/tsl/error/TslError.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Buffer.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/GLog.hxx"
#include "shared/util/Hash.hxx"
#include "shared/util/String.hxx"
#include "shared/validation/XmlValidator.hxx"
#include "shared/xml/XmlDocument.hxx"
#include "fhirtools/util/XmlHelper.hxx"

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

using namespace xmlHelperLiterals;

namespace
{
    constexpr const char* xpathLiteral_c14nAlgorithm = "/ns:TrustServiceStatusList/ds:Signature/ds:SignedInfo/ds:CanonicalizationMethod/@Algorithm";
    constexpr const char* xpathLiteral_certificate = "/ns:TrustServiceStatusList/ds:Signature/ds:KeyInfo/ds:X509Data/ds:X509Certificate/text()";
    constexpr const char* xpathLiteral_digestAlgorithm = "(/ns:TrustServiceStatusList/ds:Signature/ds:SignedInfo/ds:Reference/ds:DigestMethod)[1]/@Algorithm";
    constexpr const char* xpathLiteral_digestTransform = "/ns:TrustServiceStatusList/ds:Signature/ds:SignedInfo/ds:Reference/ds:Transforms/ds:Transform";
    constexpr const char* xpathLiteral_digestValueText = "(/ns:TrustServiceStatusList/ds:Signature/ds:SignedInfo/ds:Reference/ds:DigestValue)[1]/text()";
    constexpr const char* xpathLiteral_nextUpdateText = "/ns:TrustServiceStatusList/ns:SchemeInformation/ns:NextUpdate/ns:dateTime/text()";
    constexpr const char* xpathLiteral_tspService = "/ns:TrustServiceStatusList/ns:TrustServiceProviderList/ns:TrustServiceProvider/ns:TSPServices/ns:TSPService";
    constexpr const char* xpathLiteral_signature = "/ns:TrustServiceStatusList/ds:Signature";
    constexpr const char* xpathLiteral_signatureAlgorithm = "/ns:TrustServiceStatusList/ds:Signature/ds:SignedInfo/ds:SignatureMethod/@Algorithm";
    constexpr const char* xpathLiteral_signatureValue = "/ns:TrustServiceStatusList/ds:Signature/ds:SignatureValue/text()";
    constexpr const char* xpathLiteral_signedInfo = "/ns:TrustServiceStatusList/ds:Signature/ds:SignedInfo";
    constexpr const char* xpathLiteral_sequenceNumberText = "/ns:TrustServiceStatusList/ns:SchemeInformation/ns:TSLSequenceNumber/text()";
    constexpr const char* xpathLiteral_tslId = "/ns:TrustServiceStatusList/@Id";
    constexpr const char* xpathLiteral_otherTslPointer = "/ns:TrustServiceStatusList/ns:SchemeInformation/ns:PointersToOtherTSL/ns:OtherTSLPointer";

    constexpr const char* oid_tsl_primary_location = "1.2.276.0.76.4.120";
    constexpr const char* oid_tsl_backup_location = "1.2.276.0.76.4.121";

    constexpr const char* signatureTransformLiteral_envelopedSignatureAlgorithm = "http://www.w3.org/2000/09/xmldsig#enveloped-signature";
    constexpr const char* serviceTypeIdentifierLiteral_OCSP = "http://uri.etsi.org/TrstSvc/Svctype/Certstatus/OCSP";
    constexpr const char* serviceTypeIdentifierLiteral_OCSP_QC = "http://uri.etsi.org/TrstSvc/Svctype/Certstatus/OCSP/QC";
    constexpr const char* serviceTypeIdentifierLiteral_BNA = "http://uri.telematik/TrstSvc/Svctype/TrustedList/schemerules/DE";
    constexpr const char* serviceTypeIdentifierLiteral_TSL_CA_CHANGE = "http://uri.etsi.org/TrstSvc/Svctype/TSLServiceCertChange";

    constexpr const char* serviceInfoExtensionForeSignatures = "http://uri.etsi.org/TrstSvc/TrustedList/SvcInfoExt/ForeSignatures";

    constexpr const char* serviceStatusInaccord = "http://uri.etsi.org/TrstSvc/Svcstatus/inaccord";
    constexpr const char* serviceStatusGranted = "http://uri.etsi.org/TrstSvc/TrustedList/Svcstatus/granted";
    constexpr const char* serviceStatusUndersupervision = "http://uri.etsi.org/TrstSvc/TrustedList/Svcstatus/undersupervision";
    constexpr const char* serviceStatusSupervisionincessation = "http://uri.etsi.org/TrstSvc/TrustedList/Svcstatus/supervisionincessation";
    constexpr const char* serviceStatusAccredited = "http://uri.etsi.org/TrstSvc/TrustedList/Svcstatus/accredited";
    constexpr const char* serviceStatusRecognizedatnationallevel = "http://uri.etsi.org/TrstSvc/TrustedList/Svcstatus/recognisedatnationallevel";

    using xmlNodeCPtr = const xmlNode * const;

    std::optional<std::string> getChildAttributeValue (
        xmlNodeCPtr node,
        const std::string& attributeName)
    {
        Expects(node);
        for (xmlAttrPtr attribute=node->properties; attribute != nullptr; attribute=attribute->next)
        {
            if (attribute->type == XML_ATTRIBUTE_NODE && reinterpret_cast<const char*>(attribute->name) == attributeName)
            {
                if (attribute->children != nullptr && attribute->children->content != nullptr)
                {
                    return std::string{reinterpret_cast<const char*>(attribute->children->content)};
                }

                break;
            }
        }

        return std::nullopt;
    }


    xmlNodePtr getChildTag(const xmlNode& node, const std::string& tagName)
    {
        xmlNodePtr tag = nullptr;

        Expect (node.type == XML_ELEMENT_NODE, "parent element must be a node");
        for (xmlNodePtr child = node.children;
             child != nullptr;
             child = child->next)
        {
            if (child->type == XML_ELEMENT_NODE && child->name != nullptr && reinterpret_cast<const char*>(child->name) == tagName)
            {
                Expect(tag == nullptr, "Only one child tag with the name is expected");
                tag = child;
            }
        }

        return tag;
    }


    std::string getTagText(const xmlNode& node, const bool strict)
    {
        for (xmlNodePtr child = node.children;
             child != nullptr;
             child = child->next)
        {
            if (child->type == XML_TEXT_NODE && child->name != nullptr && strcmp(reinterpret_cast<const char*>(child->name), "text") == 0 )
            {
                Expect(child->content != nullptr, "The tag contents are expected to exist");
                return reinterpret_cast<const char*>(child->content);
            }
        }

        if (strict)
        {
            Fail2("The text for the tag must exist", std::runtime_error);
        }

        return {};
    }


    std::string getChildTagText(const xmlNode& node, const std::string& tagName, const bool strict)
    {
        xmlNodePtr tag = getChildTag(node, tagName);

        if (tag != nullptr)
        {
            return getTagText(*tag, strict);
        }

        if (strict)
        {
            Fail2("The text for the tag " + tagName + "must exist", std::runtime_error);
        }

        return {};
    }


    namespace detail
    {
    using TrustList = std::tuple<TslParser::ServiceInformationMap, TslParser::CertificateList,
                                 TslParser::BnaServiceInformation, std::vector<CertificateId>>;


    template<typename ContainerT>
    void throwIfExtractionIsEmpty(const ContainerT& container, const std::string& failureMessage)
    {
        Expect(! container.empty(), "Unable to extract: " + failureMessage);
    }


    void insertIntoTrustMap(TslParser::ServiceInformationMap& trustMap,
                            CertificateId&& key,
                            TslParser::ServiceInformation&& value)
    {
        const bool isKeyUnique =
            trustMap.emplace(std::forward<CertificateId>(key),
                             std::forward<TslParser::ServiceInformation>(value)).second;

        if (! isKeyUnique)
        {
            // key was not inserted, should still be valid. Code not compiled-in in Release build.
            //NOLINTNEXTLINE(bugprone-use-after-move,hicpp-invalid-access-moved)
            TVLOG(2) << "TSL parsing: certificate `" << key.toString()
                    << "` already present in trust map";
        }
    }


    X509Certificate getServiceCertificate(
        const xmlNode& digitalId,
        const std::string& serviceTypeIdentifier,
        const TslMode mode)
    {
        const std::string x509CertificateString = getChildTagText(digitalId, "X509Certificate", false);
        X509Certificate certificate;
        try
        {
            certificate =
                X509Certificate::createFromBase64(Base64::cleanupForDecoding(x509CertificateString));
        }
        catch(const std::runtime_error& error)
        {
            if (mode == TslMode::TSL && serviceTypeIdentifier == serviceTypeIdentifierLiteral_TSL_CA_CHANGE)
            {
                // According to TUC_PKI_013 the problem with unreadable new TSL CA must be logged,
                // but it should not affect other CAs
                TLOG(ERROR) << TslError("TI-Trust-Anchor is not a valid certificate",
                                       TslErrorCode::TSL_SIG_CERT_EXTRACTION_ERROR).what();
            }
            else
            {
                throw;
            }
        }

        return certificate;
    }


    void fillServiceSupplyPointList(TslParser::ServiceSupplyPointList& pointList, const xmlNode& serviceSupplyPoints)
    {
        for (xmlNodePtr child = serviceSupplyPoints.children; child != nullptr; child = child->next)
        {
            if (child->type == XML_ELEMENT_NODE &&
                strcmp(reinterpret_cast<const char*>(child->name), "ServiceSupplyPoint") == 0)
            {
                const std::string serviceSupplyPoint = getTagText(*child, false);
                if (! serviceSupplyPoint.empty())
                {
                    pointList.emplace_back(serviceSupplyPoint);
                }
            }
        }
    }


    bool isServiceOcsp(const TslMode mode, const std::string& serviceTypeIdentifier)
    {
        return (serviceTypeIdentifier == serviceTypeIdentifierLiteral_OCSP
                || (mode == TslMode::BNA && serviceTypeIdentifier == serviceTypeIdentifierLiteral_OCSP_QC));
    }


    bool isServiceBna(const TslMode mode,const std::string& serviceTypeIdentifier)
    {
        return (mode == TslMode::TSL && serviceTypeIdentifier == serviceTypeIdentifierLiteral_BNA);
    }


    void fillTrustedStructuresWithCertificate(
        TslParser::CertificateList& ocspCertificateList,
        TslParser::CertificateList& bnaSignerCertificateList,
        std::vector<CertificateId>& newTslSignerCaIdList,
        const TslMode mode,
        const std::string& serviceTypeIdentifier,
        const X509Certificate& certificate)
    {
        if (certificate.hasX509Ptr())
        {
            if (mode == TslMode::TSL && serviceTypeIdentifier == serviceTypeIdentifierLiteral_TSL_CA_CHANGE)
            {
                newTslSignerCaIdList.emplace_back(
                    CertificateId{certificate.getSubject(), certificate.getSubjectKeyIdentifier()});
            }

            if (isServiceOcsp(mode, serviceTypeIdentifier))
            {
                ocspCertificateList.emplace_back(certificate);
            }

            if (isServiceBna(mode, serviceTypeIdentifier))
            {
                bnaSignerCertificateList.emplace_back(certificate);
            }
        }
    }


    void fillTrustedStructuresFromDigitalId(
        TslParser::ServiceInformationMap & serviceInformationMap,
        TslParser::CertificateList& ocspCertificateList,
        TslParser::CertificateList& bnaSignerCertificateList,
        std::vector<CertificateId>& newTslSignerCaIdList,
        const xmlNode& node,
        const TslMode mode,
        const std::string& serviceTypeIdentifier,
        const TslParser::ServiceSupplyPointList& serviceSupplyPointList,
        const TslParser::AcceptanceHistoryMap& acceptanceHistoryMap,
        const TslParser::ExtensionOidList& extensionOidList)
    {
        if (node.type == XML_ELEMENT_NODE && strcmp(reinterpret_cast<const char*>(node.name), "DigitalId") == 0)
        {
            const std::string x509CertificateString = getChildTagText(node, "X509Certificate", false);
            if (! x509CertificateString.empty())
            {
                const X509Certificate certificate = getServiceCertificate(node, serviceTypeIdentifier, mode);

                if (certificate.hasX509Ptr())
                {
                    insertIntoTrustMap(
                        serviceInformationMap,
                        {certificate.getSubject(), certificate.getSubjectKeyIdentifier()},
                        TslParser::ServiceInformation{
                            certificate,
                            serviceTypeIdentifier,
                            serviceSupplyPointList,
                            acceptanceHistoryMap,
                            extensionOidList});

                    fillTrustedStructuresWithCertificate(
                        ocspCertificateList,
                        bnaSignerCertificateList,
                        newTslSignerCaIdList,
                        mode,
                        serviceTypeIdentifier,
                        certificate);
                }
            }
        }
    }


    void extendOcspMappingForExtension(const xmlNode& extensionNode, TslParser::OcspMapping& ocspMapping)
    {
        for (xmlNodePtr child = extensionNode.children; child != nullptr; child = child->next)
        {
            if (child->type == XML_ELEMENT_NODE && child->name != nullptr &&
                child->name == "AdditionalServiceInformation"_xs)
            {
                const std::string uri = getChildTagText(*child, "URI", false);
                if (uri == serviceInfoExtensionForeSignatures)
                {
                    const std::string informationValue = getChildTagText(*child, "InformationValue", false);
                    if (!informationValue.empty())
                    {
                        std::vector<std::string> informationEntries = String::split(informationValue, " ");
                        Expect(informationEntries.size() == 2, "Two entries are expected by ocsp-mapping");

                        ocspMapping.emplace(informationEntries[0], informationEntries[1]);
                    }
                }
            }
        }
    }


    TslParser::OcspMapping getBnaOcspMaping(const xmlNode& serviceInformation)
    {
        TslParser::OcspMapping ocspMapping{};

        xmlNodeCPtr serviceInformationExtension = getChildTag(serviceInformation, "ServiceInformationExtensions");
        if (serviceInformationExtension != nullptr)
        {
            for (xmlNodePtr child = serviceInformationExtension->children; child != nullptr; child = child->next)
            {
                if (child->type == XML_ELEMENT_NODE && child->name != nullptr &&
                    child->name == "Extension"_xs)
                {
                    extendOcspMappingForExtension(*child, ocspMapping);
                }
            }
        }

        return ocspMapping;
    }


    bool isServiceAccepted(const TslMode mode, const xmlNode& serviceStatusContainer)
    {
        const std::string serviceStatus =
            getChildTagText(serviceStatusContainer, "ServiceStatus", true);

        if (mode == TslMode::TSL)
        {
            return (serviceStatus == serviceStatusInaccord);
        }
        else
        {
            return (serviceStatus == serviceStatusGranted
                    || serviceStatus == serviceStatusSupervisionincessation
                    || serviceStatus == serviceStatusUndersupervision
                    || serviceStatus == serviceStatusAccredited
                    || serviceStatus == serviceStatusRecognizedatnationallevel);
        }
    }


    void extendMapWithHistoryInstance(
        const xmlNode& child,
        const TslMode mode,
        TslParser::AcceptanceHistoryMap& mapToExtend,
        const bool expectToBeTheLatest)
    {
        auto statusStartingTime = model::Timestamp::fromXsDateTime(
            String::trim(getChildTagText(child, "StatusStartingTime", true))).toChronoTimePoint();
        Expect(
            ! expectToBeTheLatest
            || mapToExtend.empty()
            || statusStartingTime > mapToExtend.rbegin()->first,
            "The status starting time is expected to be the latest");

        mapToExtend.emplace(statusStartingTime, isServiceAccepted(mode, child));
    }


    std::string getExtensionOid(const xmlNode& node)
    {
        if (node.type == XML_ELEMENT_NODE && node.name == "Extension"_xs)
        {
            for (xmlNodePtr child = node.children; child != nullptr;
                 child = child->next)
            {
                if (child->type == XML_ELEMENT_NODE && child->name == "ExtensionOID"_xs)
                {
                    return getTagText(*child, false);
                }
            }
        }

        return {};
    }


    TslParser::ExtensionOidList getExtensionOids(const xmlNode& serviceInformation)
    {
        TslParser::ExtensionOidList oids;

        xmlNodeCPtr serviceSupplyPoints =
            getChildTag(serviceInformation, "ServiceInformationExtensions");
        if (serviceSupplyPoints != nullptr)
        {
            for (xmlNodePtr child = serviceSupplyPoints->children; child != nullptr;
                 child = child->next)
            {
                const std::string oid = getExtensionOid(*child);
                if (!oid.empty())
                {
                    oids.emplace(oid);
                }
            }
        }

        return oids;
    }


    TslParser::ServiceSupplyPointList getServiceSupplyPoints(const xmlNode& serviceInformation)
    {
        TslParser::ServiceSupplyPointList serviceSupplyPointList{};
        xmlNodeCPtr serviceSupplyPoints =
            getChildTag(serviceInformation, "ServiceSupplyPoints");
        if (serviceSupplyPoints != nullptr)
        {
            fillServiceSupplyPointList(serviceSupplyPointList, *serviceSupplyPoints);
        }

        return serviceSupplyPointList;
    }


    /**
     * Fills the trust structures from the service information of TSPService,
     * returns serviceTypeIdentifier of the service.
     */
    void fillTrustStructuresFromServiceInformation(
            const TslMode mode,
            TslParser::AcceptanceHistoryMap& acceptanceHistoryMap,
            TslParser::ServiceInformationMap & serviceInformationMap,
            TslParser::CertificateList& ocspCertificateList,
            TslParser::BnaServiceInformation& bnaServiceInformation,
            std::vector<CertificateId>& newTslSignerCaIdList,
            const xmlNode& serviceInformation)
        {
            extendMapWithHistoryInstance(serviceInformation, mode, acceptanceHistoryMap, true);

            xmlNodeCPtr serviceDigitalIdentity = getChildTag(serviceInformation, "ServiceDigitalIdentity");

            if (serviceDigitalIdentity != nullptr)
            {
                const std::string serviceTypeIdentifier = String::trim(
                    getChildTagText(serviceInformation, "ServiceTypeIdentifier", true));

                TslParser::ServiceSupplyPointList serviceSupplyPointList =
                    getServiceSupplyPoints(serviceInformation);
                TslExpect4(mode != TslMode::TSL || !serviceSupplyPointList.empty(),
                           "Missing SupplyPoints in TSL.",
                           TslErrorCode::SERVICESUPPLYPOINT_MISSING,
                           mode);

                if (isServiceBna(mode, serviceTypeIdentifier))
                {
                    // this is BNA service
                    bnaServiceInformation.supplyPointList.insert(
                        bnaServiceInformation.supplyPointList.end(),
                        serviceSupplyPointList.begin(),
                        serviceSupplyPointList.end());

                    bnaServiceInformation.ocspMapping = getBnaOcspMaping(serviceInformation);
                }

                TslParser::ExtensionOidList extensionOidList = getExtensionOids(serviceInformation);
                TslExpect4(mode != TslMode::TSL || !extensionOidList.empty(),
                           "Missing ServiceInformationExtensions OIDs in TSL.",
                           TslErrorCode::TSL_NOT_WELLFORMED,
                           mode);

                for (xmlNodePtr child = serviceDigitalIdentity->children; child != nullptr;
                     child = child->next)
                {
                    fillTrustedStructuresFromDigitalId(
                        serviceInformationMap,
                        ocspCertificateList,
                        bnaServiceInformation.signerCertificateList,
                        newTslSignerCaIdList,
                        *child,
                        mode,
                        serviceTypeIdentifier,
                        serviceSupplyPointList,
                        acceptanceHistoryMap,
                        extensionOidList);
                }
            }
        }


        TslParser::AcceptanceHistoryMap getHistoryMap(const xmlNode& tspService, const TslMode mode)
        {
            TslParser::AcceptanceHistoryMap result;

            xmlNodeCPtr serviceHistory = getChildTag(tspService, "ServiceHistory");
            if (serviceHistory != nullptr)
            {
                for (xmlNodePtr child = serviceHistory->children; child != nullptr;
                     child = child->next)
                {
                    if (child->type == XML_ELEMENT_NODE && child->name == "ServiceHistoryInstance"_xs)
                    {
                        extendMapWithHistoryInstance(*child, mode, result, false);
                    }
                }
            }

            return result;
        }
    }


    namespace signature
    {
        void throwOpenSslError(const std::string& message)
        {
            std::string errors;
            while (unsigned long error = ERR_get_error())
            {
                if (! errors.empty())
                {
                    errors += "; ";
                }

                errors += ERR_error_string(error, nullptr);
            }

            Fail2(message + errors, std::runtime_error);
        }


        // GEMREQ-start A_17205
        std::string getDigestName(const std::string& algorithm)
        {
            if ("http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha256" == algorithm)
            {
                return "SHA256";
            }

            if ("http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha384" == algorithm)
            {
                return "SHA384";
            }

            if ("http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha512" == algorithm)
            {
                return "SHA512";
            }

            Fail2("ECSDSA Signature algorithm is not supported: " + algorithm, std::runtime_error);
        }
        // GEMREQ-end A_17205

        std::string getRsaDigestName(const std::string& algorithm)
        {
            if ("http://www.w3.org/2001/04/xmldsig-more#rsa-sha512" == algorithm ||
                "http://www.w3.org/2007/05/xmldsig-more#sha512-rsa-MGF1" == algorithm)
            {
                return "SHA512";
            }
            if ("http://www.w3.org/2001/04/xmldsig-more#rsa-sha384" == algorithm ||
                "http://www.w3.org/2007/05/xmldsig-more#sha384-rsa-MGF1" == algorithm)
            {
                return "SHA384";
            }
            if ("http://www.w3.org/2001/04/xmldsig-more#rsa-sha256" == algorithm ||
                "http://www.w3.org/2007/05/xmldsig-more#sha256-rsa-MGF1" == algorithm)
            {
                return "SHA256";
            }

            Fail2("Signature algorithm is not using a supported RSA digest", std::runtime_error);
        }

        int getRsaPadding(const std::string& algorithm)
        {
            if ("http://www.w3.org/2007/05/xmldsig-more#sha256-rsa-MGF1" == algorithm ||
                "http://www.w3.org/2007/05/xmldsig-more#sha384-rsa-MGF1" == algorithm ||
                "http://www.w3.org/2007/05/xmldsig-more#sha512-rsa-MGF1" == algorithm)
            {
                return RSA_PKCS1_PSS_PADDING;
            }
            if ("http://www.w3.org/2001/04/xmldsig-more#rsa-sha256" == algorithm ||
                "http://www.w3.org/2001/04/xmldsig-more#rsa-sha384" == algorithm ||
                "http://www.w3.org/2001/04/xmldsig-more#rsa-sha512" == algorithm)
            {
                return RSA_PKCS1_PADDING;
            }

            Fail2("Signature algorithm is not using a supported RSA digest", std::runtime_error);
        }


        // @see https://www.w3.org/TR/xmldsig-core/#sec-SignatureValidation
        bool validateSignature(const std::string& message, const util::Buffer& signature,
                               const std::string& algorithm, const X509Certificate& signerCertificate)
        {
            // GEMREQ-start A_17205
            bool isDomainParameterBrainpoolP256r1 = false;

            EVP_PKEY* publicKey = signerCertificate.getPublicKey();
            if (publicKey)
            {
                const auto* ecKey = EVP_PKEY_get0_EC_KEY(publicKey);
                if (ecKey)
                {
                    const auto* group = EC_KEY_get0_group(ecKey);
                    if (group && NID_brainpoolP256r1 == EC_GROUP_get_curve_name(group))
                    {
                        isDomainParameterBrainpoolP256r1 = true;
                    }
                }
            }

            if (! isDomainParameterBrainpoolP256r1)
            {
                TVLOG(1) << "refusing to validate non-BrainpoolP256r1-based ECC TSL signature";
                return false;
            }
            // GEMREQ-end A_17205

            std::shared_ptr<EVP_MD_CTX> digestContext{EVP_MD_CTX_create(), EVP_MD_CTX_free};
            if (! digestContext)
            {
                throwOpenSslError("can not create digest context");
            }

            const std::string digestName = getDigestName(algorithm);
            EVP_PKEY_CTX* publicKeyContext = nullptr;
            int status =
                EVP_DigestVerifyInit(digestContext.get(), &publicKeyContext,
                                     EVP_get_digestbyname(digestName.c_str()), nullptr, publicKey);
            if (status != 1)
            {
                throwOpenSslError("Can't initialize digest verification: ");
            }

            status = EVP_DigestVerifyUpdate(digestContext.get(), message.data(), message.length());
            if (status != 1)
            {
                throwOpenSslError("Can't update digest verification: ");
            }

            status = EVP_DigestVerifyFinal(digestContext.get(), signature.data(), signature.size());
            return status == 1;
        }


        bool verifyDigestTransforms(XmlDocument& xmlDocument)
        {
            const auto transforms = xmlDocument.getXpathObjects(xpathLiteral_digestTransform);
            for (auto index = 0; index < transforms->nodesetval->nodeNr; ++index)
            {
                const std::optional<std::string> algorithm =
                    getChildAttributeValue(transforms->nodesetval->nodeTab[index], "Algorithm");
                if (! algorithm)
                {
                    Fail2("digest transform has no Algorithm attribute", std::runtime_error);
                }

                if (*algorithm == signatureTransformLiteral_envelopedSignatureAlgorithm)
                {
                    return true;
                }
            }

            return false;
        }


        void verifyDigest(XmlDocument& xmlDocument)
        {
            const std::optional<std::string> canonicalizedData = C14NHelper::canonicalize(
                xmlDocument.getNode("/"),
                verifyDigestTransforms(xmlDocument) ? xmlDocument.getNode(xpathLiteral_signature)
                                                    : nullptr,
                &xmlDocument.getDocument(), XML_C14N_EXCLUSIVE_1_0, std::vector<std::string>{}, false);

            Expect(canonicalizedData.has_value(),
                   "the provided signed info can not be canonicalized without signature");

            const std::string expectedDigestValue =
                xmlDocument.getElementText(xpathLiteral_digestValueText);
            Expect(! expectedDigestValue.empty(), "no expected digest value is provided");
            TVLOG(2) << "Digest stored in TSL-File is [" << expectedDigestValue << "]";

            const std::string digestAlgorithm =
                xmlDocument.getAttributeValue(xpathLiteral_digestAlgorithm);
            const std::string expectedDigest = Base64::decodeToString(expectedDigestValue, true);
            std::string calculatedDigest;
            if (digestAlgorithm == "http://www.w3.org/2001/04/xmlenc#sha256")
            {
                calculatedDigest = Hash::sha256(*canonicalizedData);
            }
            else if (digestAlgorithm == "http://www.w3.org/2001/04/xmlenc#sha384")
            {
                calculatedDigest = Hash::sha384(*canonicalizedData);
            }
            else if (digestAlgorithm == "http://www.w3.org/2001/04/xmlenc#sha512")
            {
                calculatedDigest = Hash::sha512(*canonicalizedData);
            }
            else
            {
                Fail("unsupported digest algorithm: " + digestAlgorithm);
            }

            Expect(expectedDigest == calculatedDigest, "Digest value mismatch");
        }


        void validateRsaSignature(
            const std::string& canonicalizedSignedInfo,
            const util::Buffer& signature,
            const std::string& algorithm,
            const X509Certificate& signerCertificate)
        {
            EVP_PKEY* pKey = signerCertificate.getPublicKey();
            Expect(pKey != nullptr, "can not retrieve signer certificate public key");

            std::shared_ptr<EVP_MD_CTX> rsaVerifyCtx(EVP_MD_CTX_create(), EVP_MD_CTX_free);
            Expect(rsaVerifyCtx != nullptr, "Can not create verification context.");
            const std::string digestName = getRsaDigestName(algorithm);
            EVP_PKEY_CTX* keyContext = nullptr;
            Expect(EVP_DigestVerifyInit(rsaVerifyCtx.get(), &keyContext, EVP_get_digestbyname(digestName.c_str()),
                                        nullptr, pKey) == 1,
                   "Can not initialize Digest Verification.");

            Expect(EVP_PKEY_CTX_set_rsa_padding(keyContext, getRsaPadding(algorithm)) == 1,
                   "Can not set RSA padding.");

            Expect(EVP_DigestVerifyUpdate(rsaVerifyCtx.get(),
                                          canonicalizedSignedInfo.c_str(),
                                          canonicalizedSignedInfo.size()) == 1,
                   "Can not proceed with Digest verification");

            Expect(EVP_DigestVerifyFinal(rsaVerifyCtx.get(), signature.data(), signature.size()) == 1,
                   "Digest is invalid");
        }


        void verifySignature(XmlDocument& xmlDocument, const X509Certificate& signerCertificate)
        {
            const std::string canonicalizationAlgorithm =
                xmlDocument.getAttributeValue(xpathLiteral_c14nAlgorithm);
            Expect(canonicalizationAlgorithm == "http://www.w3.org/2001/10/xml-exc-c14n#",
                   "Unexpected canonicalization algorithm " + canonicalizationAlgorithm);

            const auto canonicalizedSignedInfo = C14NHelper::canonicalize(
                xmlDocument.getNode(xpathLiteral_signedInfo), nullptr, &xmlDocument.getDocument(),
                XML_C14N_EXCLUSIVE_1_0, std::vector<std::string>{}, false);

            Expect(canonicalizedSignedInfo.has_value(), "Unable to canonicalize signed info");

            // GEMREQ-start A_17206
            // GEMREQ-start A_17205
            const std::string signatureString = xmlDocument.getElementText(xpathLiteral_signatureValue);
            const std::string signatureAlgorithm = xmlDocument.getAttributeValue(xpathLiteral_signatureAlgorithm);
            TVLOG(2) << "Signature stored int TSL-File is [" << signatureString << "]";

            const util::Buffer signature = Base64::decode(Base64::cleanupForDecoding(signatureString));

            if (signerCertificate.getSigningAlgorithm() == SigningAlgorithm::ellipticCurve)
            {
                if (! validateSignature(*canonicalizedSignedInfo,
                                        EllipticCurveUtils::convertSignatureFormat(
                                            signature, EllipticCurveUtils::SignatureFormat::XMLDSIG,
                                            EllipticCurveUtils::SignatureFormat::ASN1_DER),
                                        signatureAlgorithm, signerCertificate))
                {
                    Fail2("Invalid signature", std::runtime_error);
                }
            }
            // GEMREQ-end A_17205
            // GEMREQ-end A_17206
            else if (signerCertificate.getSigningAlgorithm() == SigningAlgorithm::rsaPss)
            {
                validateRsaSignature(*canonicalizedSignedInfo, signature, signatureAlgorithm, signerCertificate);
            }
            else
            {
                Fail2("unexpected TSL signature algorithm ", std::runtime_error);
            }
        }
    }


    void validateDocumentSchema(const XmlDocument& xmlDocument, const TslMode mode, const XmlValidator& xmlValidator)
    {
        try
        {
            std::unique_ptr<XmlValidatorContext> validatorContext =
                xmlValidator.getSchemaValidationContext(
                    mode == TslMode::TSL ? SchemaType::Gematik_tsl : SchemaType::BNA_tsl);
            Expect3(validatorContext != nullptr,
                    "Can not get validator context.",
                    std::logic_error);
            xmlDocument.validateAgainstSchema(*validatorContext);
        }
        catch(const std::runtime_error& e)
        {
            TslFail3(
                std::string("Can not parse document: ") + e.what(),
                TslErrorCode::TSL_SCHEMA_NOT_VALID,
                mode);
        }
    }


    XmlDocument createXmlDocument(const std::string& xml, const TslMode mode)
    {
        try
        {
            XmlDocument xmlDocument(xml);

            xmlDocument.registerNamespace("ns", "http://uri.etsi.org/02231/v2#");
            xmlDocument.registerNamespace("ds", "http://www.w3.org/2000/09/xmldsig#");
            xmlDocument.registerNamespace("xades", "http://uri.etsi.org/01903/v1.3.2#");

            return xmlDocument;
        }
        catch(const std::runtime_error& e)
        {
            TslFail3(
                std::string("Can not parse document: ") + e.what(),
                TslErrorCode::TSL_NOT_WELLFORMED,
                mode);
        }
    }


    std::optional<std::string> extractId(XmlDocument& xmlDocument, const TslMode mode)
    {
        try
        {
            return xmlDocument.getOptionalAttributeValue(xpathLiteral_tslId);
        }
        catch(const std::runtime_error& e)
        {
            TslFail3(
                std::string("Can not parse id: ") + e.what(),
                TslErrorCode::TSL_NOT_WELLFORMED,
                mode);
        }
    }


    std::string extractSequenceNumber(XmlDocument& xmlDocument, const TslMode mode)
    {
        try
        {
            return xmlDocument.getElementText(xpathLiteral_sequenceNumberText);
        }
        catch(const std::runtime_error& e)
        {
            TslFail3(
                std::string("Can not parse sequence number: ") + e.what(),
                TslErrorCode::TSL_NOT_WELLFORMED,
                mode);
        }
    }


    std::chrono::system_clock::time_point extractNextUpdate(XmlDocument& xmlDocument, const TslMode mode)
    {
        try
        {
            const std::string dateTime = xmlDocument.getElementText(xpathLiteral_nextUpdateText);
            // TODO: replace with implementation supporting xs:dateTime format
            //       for https://dth01.ibmgcloud.net/jira/browse/ERP-4816
            return model::Timestamp::fromFhirDateTime(String::trim(dateTime)).toChronoTimePoint();
        }
        catch(const std::runtime_error& e)
        {
            TslFail3(
                std::string("Can not parse next update: ") + e.what(),
                TslErrorCode::TSL_NOT_WELLFORMED,
                mode);
        }
    }


    X509Certificate extractSignerCertificate(XmlDocument& xmlDocument, const TslMode mode)
    {
        try
        {
            const std::string certificateBase64 = xmlDocument.getElementText(xpathLiteral_certificate);
            X509Certificate certificate =
                X509Certificate::createFromBase64(Base64::cleanupForDecoding(certificateBase64));

            Expect(certificate.checkValidityPeriod(), "Invalid signer certificate");
            return certificate;
        }
        catch(const std::runtime_error& e)
        {
            TslFail3(
                std::string("Can not parse signer certificate: ") + e.what(),
                TslErrorCode::TSL_SIG_CERT_EXTRACTION_ERROR,
                mode);
        }
    }


    TslParser::UpdateUrlList extractUpdateUrlList(XmlDocument& xmlDocument, const TslMode mode)
    {
        try
        {
            std::string primaryUrl;
            std::string backupUrl;

            std::shared_ptr<xmlXPathObject> otherTslPointer = xmlDocument.getXpathObjects(
                xpathLiteral_otherTslPointer);
            for (int index = 0; index < otherTslPointer->nodesetval->nodeNr; ++index)
            {
                xmlNodeCPtr pointer = otherTslPointer->nodesetval->nodeTab[index];
                Expect(pointer != nullptr && pointer->type == XML_ELEMENT_NODE,
                       "OtherTSLPointer must be a valid node");

                xmlNodeCPtr additionalInformation = getChildTag(*pointer, "AdditionalInformation");
                Expect(additionalInformation != nullptr, "AdditionalInformation must be provided");

                const std::string oid = getChildTagText(
                    *additionalInformation, "TextualInformation", true);

                if (oid == oid_tsl_primary_location)
                {
                    Expect(primaryUrl.empty(), "unexpected duplicated primary TSL update URL");
                    primaryUrl = getChildTagText(*pointer, "TSLLocation", true);
                }
                else if (oid == oid_tsl_backup_location)
                {
                    Expect(backupUrl.empty(), "unexpected duplicated backup TSL update URL");
                    backupUrl = getChildTagText(*pointer, "TSLLocation", true);
                }
                else
                {
                    Fail("unexpected OID is provided in TslPointer");
                }
            }

            Expect(!primaryUrl.empty() && !backupUrl.empty(),
                   "The TSL update URLs must be provided");
            return {primaryUrl, backupUrl};
        }
        catch(const std::runtime_error& e)
        {
            TslFail3(
                std::string("Can not parse update URL list: ") + e.what(),
                TslErrorCode::TSL_DOWNLOAD_ADDRESS_ERROR,
                mode);
        }
    }


    detail::TrustList extractTrustList(XmlDocument& xmlDocument, const TslMode mode)
    {
        try
        {
            TslParser::ServiceInformationMap serviceInformationMap{};
            TslParser::CertificateList ocspCertificateList{};
            TslParser::BnaServiceInformation bnaServiceInformation{};
            std::vector<CertificateId> newTslSignerCaIdList{};

            std::shared_ptr<xmlXPathObject> tspServices = xmlDocument.getXpathObjects(
                xpathLiteral_tspService);

            for (int index = 0; index < tspServices->nodesetval->nodeNr; ++index)
            {
                xmlNodeCPtr tspService = tspServices->nodesetval->nodeTab[index];
                Expect (tspService != nullptr && tspService->type == XML_ELEMENT_NODE,
                        "TspService must be a valid node");

                TslParser::AcceptanceHistoryMap historyMap = detail::getHistoryMap(*tspService, mode);

                xmlNodeCPtr serviceInformation = getChildTag(*tspService, "ServiceInformation");
                Expect (serviceInformation != nullptr, "ServiceInvormation must be a valid one");

                detail::fillTrustStructuresFromServiceInformation(
                    mode,
                    historyMap,
                    serviceInformationMap,
                    ocspCertificateList,
                    bnaServiceInformation,
                    newTslSignerCaIdList,
                    *serviceInformation);
            }

            detail::throwIfExtractionIsEmpty(serviceInformationMap, "service information");
            TslExpect4( ! ocspCertificateList.empty(),
                      "Missing OCSP certificates",
                      TslErrorCode::OCSP_CERT_MISSING,
                      mode);

            if (mode == TslMode::TSL)
            {
                // base TSL must contain BNA supply points
                detail::throwIfExtractionIsEmpty(bnaServiceInformation.supplyPointList, "BNA supply points");
                detail::throwIfExtractionIsEmpty(bnaServiceInformation.signerCertificateList, "BNA signer certificates");
            }

            return std::make_tuple(serviceInformationMap,
                                   ocspCertificateList,
                                   bnaServiceInformation,
                                   newTslSignerCaIdList);
        }
        catch(const TslError&)
        {
            throw;
        }
        catch(const std::runtime_error& e)
        {
            TslFail3(
                std::string("Can not parse trust list: ") + e.what(),
                TslErrorCode::TSL_NOT_WELLFORMED,
                mode);
        }
    }


    void validateDigestAndSignature(XmlDocument& xmlDocument, const X509Certificate& signerCertificate, const TslMode mode)
    {
        try
        {
            signature::verifyDigest(xmlDocument);
            signature::verifySignature(xmlDocument, signerCertificate);
        }
        catch (const std::exception& e)
        {
            TslFail3(
                std::string("Unable to verify TSL XML signature: ") + e.what(),
                TslErrorCode::XML_SIGNATURE_ERROR,
                mode);
        }
    }
}


TslParser::TslParser(const std::string& tslXml, const TslMode tslMode, const XmlValidator& xmlValidator)
    : mTslMode(tslMode)
    , mId{}
    , mSequenceNumber{}
    , mNextUpdate{}
    , mSignerCertificate{}
    , mOcspCertificateList{}
    , mServiceInformationMap{}
{
    performExtractions(tslXml, xmlValidator);
}


const std::optional<std::string>& TslParser::getId() const
{
    return mId;
}


const std::string& TslParser::getSequenceNumber() const
{
    return mSequenceNumber;
}


const std::chrono::system_clock::time_point& TslParser::getNextUpdate() const
{
    return mNextUpdate;
}


const X509Certificate& TslParser::getSignerCertificate() const
{
    return mSignerCertificate;
}


const TslParser::CertificateList& TslParser::getOcspCertificateList() const
{
    return mOcspCertificateList;
}


const TslParser::ServiceInformationMap & TslParser::getServiceInformationMap() const
{
    return mServiceInformationMap;
}


const TslParser::UpdateUrlList& TslParser::getUpdateUrls() const
{
    return mUpdateUrlList;
}


const TslParser::BnaServiceInformation& TslParser::getBnaServiceInformation() const
{
    return mBnaServiceInformation;
}


const std::vector<CertificateId>& TslParser::getNewTslSignerCaIdList() const
{
    return mNewTslSignerCaIdList;
}


void TslParser::performExtractions(const std::string& xml, const XmlValidator& xmlValidator)
{
    try
    {
        XmlDocument xmlDocument = createXmlDocument(xml, mTslMode);
        validateDocumentSchema(xmlDocument, mTslMode, xmlValidator);

        mId = extractId(xmlDocument, mTslMode);

        mSequenceNumber = extractSequenceNumber(xmlDocument, mTslMode);

        mNextUpdate = extractNextUpdate(xmlDocument, mTslMode);

        mSignerCertificate = extractSignerCertificate(xmlDocument, mTslMode);

        if (mTslMode != TslMode::BNA)
        {
            // BNA update URLs are provided in original TSL
            mUpdateUrlList = extractUpdateUrlList(xmlDocument, mTslMode);
        }

        std::tie(mServiceInformationMap,
                 mOcspCertificateList,
                 mBnaServiceInformation,
                 mNewTslSignerCaIdList) = extractTrustList(xmlDocument, mTslMode);

        validateDigestAndSignature(xmlDocument, mSignerCertificate, mTslMode);
    }
    catch(const TslError& e)
    {
        TLOG(ERROR) << "parsing of TSL file has failed with TslError: " << e.what();
        throw;
    }
    catch(const std::exception& e)
    {
        TLOG(ERROR) << "parsing of TSL file has failed with unexpected exception: " << e.what();
        throw;
    }
    catch(...)
    {
        TLOG(ERROR) << "parsing of TSL file has failed";
        throw;
    }
}

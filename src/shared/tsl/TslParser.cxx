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
#include <mutex>// for call_once
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <xmlsec/crypto.h>
#include <xmlsec/errors.h>
#include <xmlsec/openssl/evp.h>
#include <xmlsec/xmldsig.h>
#include <xmlsec/xmlsec.h>
#include <xmlsec/xmltree.h>

using namespace xmlHelperLiterals;

namespace
{
    constexpr const char* xpathLiteral_certificate = "/ns:TrustServiceStatusList/ds:Signature/ds:KeyInfo/ds:X509Data/ds:X509Certificate/text()";
    constexpr const char* xpathLiteral_nextUpdateText = "/ns:TrustServiceStatusList/ns:SchemeInformation/ns:NextUpdate/ns:dateTime/text()";
    constexpr const char* xpathLiteral_tspService = "/ns:TrustServiceStatusList/ns:TrustServiceProviderList/ns:TrustServiceProvider/ns:TSPServices/ns:TSPService";
    constexpr const char* xpathLiteral_signatureValue = "/ns:TrustServiceStatusList/ds:Signature/ds:SignatureValue/text()";
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
        std::once_flag xmlsecInitialized; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

        void xmlsecErrorCallback(const char* file, int line, const char* func, const char* errorObject,
                             const char* /*errorSubject*/, int /*reason*/, const char* msg)
        {
            LOG(WARNING) << "[xmlsec error] " << (msg != nullptr ? msg : "unknown error") << " in "
                         << (func != nullptr ? func : "unknown") << " at " << (file != nullptr ? file : "file") << ":"
                         << line << ", object " << errorObject;
        }

        void verifySignatureXmlSec(XmlDocument& xmlDocument, const X509Certificate& signerCertificate)
        {
            std::call_once(xmlsecInitialized, [] {
                Expect(xmlSecInit() >= 0, "Unable to initialize xmlsec");
                Expect(xmlSecCheckVersion() == 1, "Unexpected xmlsec version");
                Expect(xmlSecCryptoAppInit(nullptr) >= 0, "Could not init xmlsec crypto app");
                Expect(xmlSecCryptoInit() >= 0, "Could not init xmlsec crypto");
            });

            static const std::unordered_set<XmlStringView> allowedSignatureMethods{
                XmlStringView{xmlSecNameEcdsaSha256},   XmlStringView{xmlSecNameEcdsaSha384},
                XmlStringView{xmlSecNameEcdsaSha512},   XmlStringView{xmlSecNameEcdsaSha3_256},
                XmlStringView{xmlSecNameEcdsaSha3_384}, XmlStringView{xmlSecNameEcdsaSha3_512},
                XmlStringView{xmlSecNameRsaSha256},     XmlStringView{xmlSecNameRsaSha384},
                XmlStringView{xmlSecNameRsaSha512},     XmlStringView{xmlSecNameRsaPssSha256},
                XmlStringView{xmlSecNameRsaPssSha384},  XmlStringView{xmlSecNameRsaPssSha512}};
            static const std::unordered_set<XmlStringView> allowedDigests{
                XmlStringView{xmlSecNameSha256}, XmlStringView{xmlSecNameSha384}, XmlStringView{xmlSecNameSha512}};

            xmlSecErrorsSetCallback(xmlsecErrorCallback);
            std::unique_ptr<xmlSecDSigCtx, decltype(&xmlSecDSigCtxDestroy)> dsigCtx(xmlSecDSigCtxCreate(nullptr),
                                                                                    &xmlSecDSigCtxDestroy);
            Expect(dsigCtx != nullptr, "Could not create dsig ctx");
            auto* evpPkey = signerCertificate.getPublicKey();
            Expect(EVP_PKEY_up_ref(evpPkey) == 1, "EVP_PKEY_up_ref failed");
            auto pubkey = shared_EVP_PKEY::make(evpPkey);
            std::unique_ptr<xmlSecKeyData, decltype(&xmlSecKeyDataDestroy)> secKeyData(
                xmlSecOpenSSLEvpKeyAdopt(pubkey.get()), &xmlSecKeyDataDestroy);
            Expect(secKeyData != nullptr, "xmlSecOpenSSLEvpKeyAdopt failed");
            pubkey.release();// ownership got transferred NOLINT(bugprone-unused-return-value)
            std::unique_ptr<xmlSecKey, decltype(&xmlSecKeyDestroy)> secKey(xmlSecKeyCreate(), &xmlSecKeyDestroy);
            Expect(secKey != nullptr, "Unable to create xmlSecKey");
            auto ret = xmlSecKeySetValue(secKey.get(), secKeyData.get());
            Expect(ret >= 0, "xmlSecKeySetValue failed");
            secKeyData.release();           // NOLINT(bugprone-unused-return-value)
            dsigCtx->signKey = secKey.get();// passes ownership to dsigCtx
            secKey.release();               // NOLINT(bugprone-unused-return-value)
            // Verify signature
            xmlNodePtr node =
                xmlSecFindNode(xmlDocGetRootElement(&xmlDocument.getDocument()), xmlSecNodeSignature, xmlSecDSigNs);
            Expect(xmlSecDSigCtxVerify(dsigCtx.get(), node) >= 0, "Signature validation could not be performed");

            const auto numSignedInfos = xmlSecPtrListGetSize(&(dsigCtx->signedInfoReferences));
            Expect(numSignedInfos > 0, "At least one reference is expected.");
            bool foundRootUri = false;
            for (size_t i = 0; i < numSignedInfos; ++i)
            {
                auto* dsigRefCtx =
                    static_cast<xmlSecDSigReferenceCtxPtr>(xmlSecPtrListGetItem(&(dsigCtx->signedInfoReferences), i));
                Expect(dsigRefCtx != nullptr && dsigRefCtx->status == xmlSecDSigStatusSucceeded,
                       "Reference verification failed");
                const auto digestName = XmlStringView(xmlSecTransformGetName(dsigRefCtx->digestMethod));
                Expect(allowedDigests.contains(digestName), std::string{"Unsupported digest: "}.append(digestName));
                const auto uri =
                    std::string_view{(dsigRefCtx->uri != nullptr ? XmlStringView(dsigRefCtx->uri) : XmlStringView(""))};
                foundRootUri |= uri.empty();
            }
            Expect(foundRootUri == true, "Did not find reference for root document");
            const auto failureReason = std::string{xmlSecDSigCtxGetFailureReasonString(dsigCtx->failureReason)};
            Expect(dsigCtx->status == xmlSecDSigStatusSucceeded, "Signature did not succeed: " + failureReason);

            auto signatureMethod = XmlStringView(xmlSecTransformGetName(dsigCtx->signMethod));
            Expect(allowedSignatureMethods.contains(signatureMethod),
                   std::string{"Unsupported signature method: "}.append(signatureMethod));
        }
    } // namespace signature


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


    X509Certificate extractSignerCertificate(XmlDocument& xmlDocument, const TslMode mode,
                                             std::chrono::system_clock::time_point referenceTimePoint)
    {
        try
        {
            const std::string certificateBase64 = xmlDocument.getElementText(xpathLiteral_certificate);
            X509Certificate certificate =
                X509Certificate::createFromBase64(Base64::cleanupForDecoding(certificateBase64));

            Expect(certificate.checkValidityPeriod(referenceTimePoint), "Invalid signer certificate");
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
            signature::verifySignatureXmlSec(xmlDocument, signerCertificate);
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


TslParser::TslParser(const std::string& tslXml, const TslMode tslMode, const XmlValidator& xmlValidator, std::optional<std::chrono::system_clock::time_point> referenceTimePoint)
    : mTslMode(tslMode)
    , mId{}
    , mSequenceNumber{}
    , mNextUpdate{}
    , mSignerCertificate{}
    , mOcspCertificateList{}
    , mServiceInformationMap{}
    , mReferenceTimePoint{referenceTimePoint.value_or(std::chrono::system_clock::now())}
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


const std::string& TslParser::getSha256() const
{
    return mSha256Hash;
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
        mSha256Hash = String::toHexString(Hash::sha256(xml));
        XmlDocument xmlDocument = createXmlDocument(xml, mTslMode);
        validateDocumentSchema(xmlDocument, mTslMode, xmlValidator);

        mId = extractId(xmlDocument, mTslMode);

        mSequenceNumber = extractSequenceNumber(xmlDocument, mTslMode);

        mNextUpdate = extractNextUpdate(xmlDocument, mTslMode);

        mSignerCertificate = extractSignerCertificate(xmlDocument, mTslMode, mReferenceTimePoint);

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

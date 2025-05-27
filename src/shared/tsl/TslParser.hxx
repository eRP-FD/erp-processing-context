/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TSL_TSLPARSER_HXX
#define ERP_PROCESSING_CONTEXT_TSL_TSLPARSER_HXX

#include "shared/tsl/CertificateId.hxx"
#include "shared/tsl/TslMode.hxx"
#include "shared/tsl/X509Certificate.hxx"

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class XmlValidator;

class TslParser
{
public:
    explicit TslParser(const std::string& tslXml, const TslMode tslMode, const XmlValidator& xmlValidator);

    TslParser(const TslParser&) = delete;

    TslParser(TslParser&&) = default;

    using CertificateList = std::vector<X509Certificate>;
    using ServiceSupplyPointList = std::vector<std::string>;
    using UpdateUrlList = std::vector<std::string>;
    using ExtensionOidList = std::unordered_set<std::string>;
    using OcspMapping = std::unordered_map<std::string, std::string>;
    using AcceptanceHistoryMap = std::map<std::chrono::system_clock::time_point, bool>;

    class ServiceInformation
    {
    public:
        X509Certificate certificate;
        std::string serviceIdentifier;
        ServiceSupplyPointList serviceSupplyPointList;
        AcceptanceHistoryMap serviceAcceptanceHistory;
        ExtensionOidList extensionOidList;
    };

    class BnaServiceInformation
    {
    public:
        /**
         * The ocsp-mapping defined in BNA service.
         */
        OcspMapping ocspMapping;

        /**
          * The list of supply points defined in BNA service.
          */
        ServiceSupplyPointList supplyPointList;

        /**
         * The list of expected BNA signer Certificates.
         */
        CertificateList signerCertificateList;
    };

    using ServiceInformationMap = std::unordered_map<CertificateId, ServiceInformation>;

    const std::optional<std::string>& getId() const;

    const std::string& getSequenceNumber() const;

    const std::chrono::system_clock::time_point& getNextUpdate() const;

    const X509Certificate& getSignerCertificate() const;

    const CertificateList& getOcspCertificateList() const;

    const ServiceInformationMap& getServiceInformationMap() const;

    /**
     * Return the hex of the sha256 sum of the passed TSL
     */
    const std::string& getSha256() const;

    /**
     * Returns the list of update urls for this TSL,
     * first element must be the primary URL,
     * second element must be backup URL
     */
    const UpdateUrlList& getUpdateUrls() const;

    /**
     * Returns the list of URL links to BNetzA-VL.
     */
    const ServiceSupplyPointList&  getBnaUrls() const;

    /**
     * Returns the information related to BNA service.
     */
    const BnaServiceInformation& getBnaServiceInformation() const;

    const std::vector<CertificateId>& getNewTslSignerCaIdList() const;

private:
    void performExtractions(const std::string& xml, const XmlValidator& xmlValidator);

    const TslMode mTslMode;
    std::optional<std::string> mId;
    std::string mSequenceNumber;
    std::chrono::system_clock::time_point mNextUpdate;
    X509Certificate mSignerCertificate;
    CertificateList mOcspCertificateList;
    ServiceInformationMap mServiceInformationMap;
    std::vector<CertificateId> mNewTslSignerCaIdList;
    std::string mSha256Hash;

    BnaServiceInformation mBnaServiceInformation;

    UpdateUrlList mUpdateUrlList;
};

#endif

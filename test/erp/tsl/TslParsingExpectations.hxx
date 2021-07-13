#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_TSL_TSLPARSINGEXPECTATIONS_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_TSL_TSLPARSINGEXPECTATIONS_HXX

#include "erp/tsl/X509Certificate.hxx"
#include "erp/tsl/TslParser.hxx"

#include <string>
#include <chrono>

class TslParsingExpectations
{
public:
    static const std::string expectedId;
    static const std::string expectedSequenceNumber;
    static const std::chrono::system_clock::time_point expectedNextUpdate;
    static const X509Certificate expectedSignerCertificate;
    static const TslParser::CertificateList expectedOcspCertificateList;
    static const TslParser::ServiceInformationMap expectedServiceInformationMap;
};


#endif

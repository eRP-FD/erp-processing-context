/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_TSL_TSLPARSINGEXPECTATIONS_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_TSL_TSLPARSINGEXPECTATIONS_HXX

#include "shared/tsl/X509Certificate.hxx"
#include "shared/tsl/TslParser.hxx"

#include <string>
#include <chrono>

class TslParsingExpectations
{
public:
    static const std::string expectedId;
    static const std::string expectedSequenceNumber;
    static const std::chrono::system_clock::time_point expectedNextUpdate;
    static const TslParser::CertificateList expectedOcspCertificateList;
    static const TslParser::ServiceInformationMap expectedServiceInformationMap;

    static X509Certificate getExpectedSignerCertificate();
};


#endif

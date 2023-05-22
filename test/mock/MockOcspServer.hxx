/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef TEST_ERP_PROCESSING_CONTEXT_UT_TSL_MOCKOCSPSERVER_HXX
#define TEST_ERP_PROCESSING_CONTEXT_UT_TSL_MOCKOCSPSERVER_HXX

#include "erp/server/HttpsServer.hxx"
#include "erp/util/Configuration.hxx"
#include "mock/tsl/MockOcsp.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "test/util/StaticData.hxx"

#include <memory>


class MockOcspServer
{
public:
    /// Factory method to create an HttpsServer that will handle incoming signing requests
    static std::unique_ptr<HttpsServer> create (
        const std::string& hostIp,
        uint16_t port,
        const std::vector<MockOcsp::CertificatePair>& ocspResponderKnownCertificateCaPairs);
};


#endif

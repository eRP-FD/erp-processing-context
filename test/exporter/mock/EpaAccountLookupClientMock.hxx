/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_EXPORTER_MOCK_EPAACCOUNTLOOKUPCLIENTMOCK_HXX
#define ERP_PROCESSING_CONTEXT_TEST_EXPORTER_MOCK_EPAACCOUNTLOOKUPCLIENTMOCK_HXX


#include "exporter/EpaAccountLookupClient.hxx"
class EpaAccountLookupClientMock : public IEpaAccountLookupClient
{
public:
    EpaAccountLookupClientMock& setResponseStatus(HttpStatus httpStatus);
    EpaAccountLookupClientMock& setResponseBody(std::string_view responseBody);

    ClientResponse sendConsentDecisionsRequest(const model::Kvnr& kvnr, const std::string& host,
                                               uint16_t port) override;

private:
    HttpStatus mHttpStatus{HttpStatus::OK};
    std::string mResponseBody;
};


#endif//ERP_PROCESSING_CONTEXT_TEST_EXPORTER_MOCK_EPAACCOUNTLOOKUPCLIENTMOCK_HXX
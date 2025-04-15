/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
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

    ClientResponse sendConsentDecisionsRequest(const std::string& xRequestId, const model::Kvnr& kvnr,
                                               const std::string& host, uint16_t port) override;

    IEpaAccountLookupClient& addLogAttribute(const std::string& key, const std::any& value) override
    {
        (void) key;
        (void) value;
        return *this;
    }

private:
    HttpStatus mHttpStatus{HttpStatus::OK};
    std::string mResponseBody;
};


#endif//ERP_PROCESSING_CONTEXT_TEST_EXPORTER_MOCK_EPAACCOUNTLOOKUPCLIENTMOCK_HXX

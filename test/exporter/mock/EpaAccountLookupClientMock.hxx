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

    IEpaAccountLookupClient& addLogAttribute(const BDEMessage::Data& data) override
    {
        mBdeData.merge(data);
        return *this;
    }

    virtual const BDEMessage::Data& getLogAttributes() const
    {
        return mBdeData;
    }

private:
    HttpStatus mHttpStatus{HttpStatus::OK};
    std::string mResponseBody;
    BDEMessage::Data mBdeData;
};

#endif//ERP_PROCESSING_CONTEXT_TEST_EXPORTER_MOCK_EPAACCOUNTLOOKUPCLIENTMOCK_HXX

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_EXPORTER_MOCK_EPAACCOUNTLOOKUPMOCK_HXX
#define ERP_PROCESSING_CONTEXT_TEST_EXPORTER_MOCK_EPAACCOUNTLOOKUPMOCK_HXX


#include "EpaAccountLookupClientMock.hxx"
#include "exporter/EpaAccountLookup.hxx"
class EpaAccountLookupMock : public IEpaAccountLookup
{
public:
    EpaAccountLookupMock(const EpaAccount& epaAccount);

    EpaAccount lookup(const std::string& xRequestId, const model::Kvnr& kvnr) override;
    EpaAccount lookup(const std::string& xRequestId, const model::Kvnr& kvnr,
                              const std::vector<std::tuple<std::string, uint16_t>>& epaAsHostPortList) override;
    EpaAccount::Code checkConsent(const std::string& xRequestId, const model::Kvnr& kvnr, const std::string& host, uint16_t port) override;
    IEpaAccountLookupClient& lookupClient() override;


  EpaAccount epaAccountLookupResult;
  EpaAccountLookupClientMock mockLookupClient;
  std::string xRequestId;

};


#endif//ERP_PROCESSING_CONTEXT_TEST_EXPORTER_MOCK_EPAACCOUNTLOOKUPMOCK_HXX

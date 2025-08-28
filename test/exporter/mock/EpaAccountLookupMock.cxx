/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "EpaAccountLookupMock.hxx"


EpaAccountLookupMock::EpaAccountLookupMock(const EpaAccount& epaAccount)
    : epaAccountLookupResult(epaAccount)
{
}

EpaAccount EpaAccountLookupMock::lookup(const std::string& xRequestId, const model::Kvnr& kvnr,
                                        const std::optional<std::string>& prefix /* = std::nullopt */)
{
    this->xRequestId = xRequestId;
    epaAccountLookupResult.kvnr = kvnr;
    epaAccountLookupResult.host = prefix.value_or(WellKnownHost);
    return epaAccountLookupResult;
}

EpaAccount EpaAccountLookupMock::lookup(const std::string& xRequestId, const model::Kvnr& kvnr,
                                        const std::vector<std::tuple<std::string, uint16_t>>& )
{
    this->xRequestId = xRequestId;
    epaAccountLookupResult.kvnr = kvnr;
    return epaAccountLookupResult;
}

EpaAccount::Code EpaAccountLookupMock::checkConsent(const std::string& xRequestId, const model::Kvnr& kvnr, const std::string& , uint16_t )
{
    this->xRequestId = xRequestId;
    epaAccountLookupResult.kvnr = kvnr;
    return epaAccountLookupResult.lookupResult;
}

IEpaAccountLookupClient& EpaAccountLookupMock::lookupClient()
{
    return mockLookupClient;
}

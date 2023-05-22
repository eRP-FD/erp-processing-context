/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "MockConsentTable.hxx"

#include "erp/util/Expect.hxx"
#include "erp/model/Consent.hxx"

#include <pqxx/except>

void MockConsentTable::storeConsent(const db_model::HashedKvnr& kvnr, const model::Timestamp& creationTime)
{
    bool inserted{};
    std::tie(std::ignore, inserted) = mConsents.emplace(kvnr, creationTime);
    if (!inserted)
    {
        throw pqxx::unique_violation(R"(ERROR:  duplicate key value violates unique constraint "consent_pkey")");
    }
}

std::optional<model::Timestamp> MockConsentTable::getConsentDateTime(const db_model::HashedKvnr& kvnr)
{
    auto dbConsent = mConsents.find(kvnr);
    if (dbConsent == mConsents.end())
    {
        return std::nullopt;
    }
    return dbConsent->second;
}

bool MockConsentTable::clearConsent(const db_model::HashedKvnr& kvnr)
{
    return mConsents.erase(kvnr) > 0;
}

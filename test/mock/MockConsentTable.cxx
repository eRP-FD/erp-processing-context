/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "MockConsentTable.hxx"

#include "shared/util/Expect.hxx"
#include "erp/model/Consent.hxx"

#include <pqxx/except>

void MockConsentTable::storeConsent(const db_model::HashedKvnr& kvnr, const model::Timestamp& creationTime, db_model::ConsentCategory category)
{
    bool inserted{};
    std::tie(std::ignore, inserted) = mConsents.emplace(make_pair(kvnr, category), creationTime);
    if (!inserted)
    {
        throw pqxx::unique_violation(R"(ERROR:  duplicate key value violates unique constraint "consent_pkey")");
    }
}

std::optional<model::Timestamp> MockConsentTable::getConsentDateTime(const db_model::HashedKvnr& kvnr,
                                                                     db_model::ConsentCategory category)
{
    auto dbConsent = mConsents.find(make_pair(kvnr, category));
    if (dbConsent == mConsents.end())
    {
        return std::nullopt;
    }
    return dbConsent->second;
}

std::vector<db_model::Consent> MockConsentTable::all(const db_model::HashedKvnr& kvnr)
{
    std::vector<db_model::Consent> ret;
    for (auto& consent : mConsents)
    {
        if (consent.first.first == kvnr)
        {
            ret.emplace_back(consent.first.second, consent.second);
        }
    }
    return ret;
}

bool MockConsentTable::clearConsent(const db_model::HashedKvnr& kvnr, db_model::ConsentCategory category)
{
    return mConsents.erase(make_pair(kvnr, category)) > 0;
}

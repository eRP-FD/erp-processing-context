/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_MOCKCONSENTTABLE_HXX
#define ERP_PROCESSING_CONTEXT_TEST_MOCKCONSENTTABLE_HXX

#include "erp/database/ErpDatabaseModel.hxx"
#include "shared/database/DatabaseModel.hxx"
#include "shared/model/Timestamp.hxx"

#include <map>

class MockConsentTable
{
public:
    void storeConsent(const db_model::HashedKvnr& kvnr, const model::Timestamp& creationTime,
                      db_model::ConsentCategory category);
    std::optional<model::Timestamp> getConsentDateTime(const db_model::HashedKvnr& kvnr,
                                                       db_model::ConsentCategory category);
    std::vector<db_model::Consent> all(const db_model::HashedKvnr& kvnr);
    bool clearConsent(const db_model::HashedKvnr& kvnr, db_model::ConsentCategory category);

private:
    std::map<std::pair<db_model::HashedKvnr, const db_model::ConsentCategory>, model::Timestamp> mConsents;

};

#endif// ERP_PROCESSING_CONTEXT_TEST_MOCKCONSENTTABLE_HXX

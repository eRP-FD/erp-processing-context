/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_MOCKCONSENTTABLE_HXX
#define ERP_PROCESSING_CONTEXT_TEST_MOCKCONSENTTABLE_HXX

#include "erp/database/DatabaseModel.hxx"


#include <map>

class MockConsentTable
{
public:
    void storeConsent(const db_model::HashedKvnr& kvnr, const model::Timestamp& creationTime);
    std::optional<model::Timestamp> getConsentDateTime(const db_model::HashedKvnr & kvnr);
    bool clearConsent(const db_model::HashedKvnr& kvnr);

private:
    std::map<db_model::HashedKvnr, model::Timestamp> mConsents;

};

#endif// ERP_PROCESSING_CONTEXT_TEST_MOCKCONSENTTABLE_HXX

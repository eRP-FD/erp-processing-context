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
    void storeConsent(const db_model::HashedKvnr& kvnr, const fhirtools::Timestamp& creationTime);
    std::optional<fhirtools::Timestamp> getConsentDateTime(const db_model::HashedKvnr & kvnr);
    bool clearConsent(const db_model::HashedKvnr& kvnr);

private:
    std::map<db_model::HashedKvnr, fhirtools::Timestamp> mConsents;

};

#endif// ERP_PROCESSING_CONTEXT_TEST_MOCKCONSENTTABLE_HXX

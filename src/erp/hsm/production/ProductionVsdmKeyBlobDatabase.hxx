/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_PRODUCTIONVSDMKEYBLOBDATABASE_HXX
#define ERP_PROCESSING_CONTEXT_PRODUCTIONVSDMKEYBLOBDATABASE_HXX

#include "erp/hsm/VsdmKeyBlobDatabase.hxx"
#include "erp/database/PostgresConnection.hxx"

#include <mutex>

/**
 * The production version of the VSDM Key blob database where all data is stored and
 * retrieved from a PostgreSQL database.
 */
class ProductionVsdmKeyBlobDatabase : public VsdmKeyBlobDatabase
{
public:
    ProductionVsdmKeyBlobDatabase();
    explicit ProductionVsdmKeyBlobDatabase(const std::string& connectionString);
    Entry getBlob(char operatorId, char version) const override;
    std::vector<Entry> getAllBlobs() const override;

    void storeBlob(Entry&& entry) override;

    void deleteBlob(char operatorId, char version) override;

    void recreateConnection() override;

private:
    static ProductionVsdmKeyBlobDatabase::Entry entryFromRow(const pqxx::row& rowEntry);

    void commitTransaction();
    mutable std::mutex mMutex;

    mutable PostgresConnection mConnection;
};

#endif
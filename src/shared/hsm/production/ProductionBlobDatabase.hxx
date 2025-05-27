/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_PRODUCTIONBLOBDATABASE_HXX
#define ERP_PROCESSING_CONTEXT_PRODUCTIONBLOBDATABASE_HXX

#include "shared/enrolment/EnrolmentData.hxx"

#include "shared/database/PostgresConnection.hxx"
#include "shared/hsm/BlobDatabase.hxx"

#include <mutex>
#include <optional>
#include <pqxx/transaction_base>
#include <unordered_map>
#include <vector>

namespace pqxx {class connection;}


/**

 */
class ProductionBlobDatabase : public BlobDatabase
{
public:
    ProductionBlobDatabase();
    explicit ProductionBlobDatabase (const std::string& connectionString);

    Entry getBlob (
        BlobType type,
        BlobId id) const override;
    Entry getBlob(BlobType type, const ErpVector& name) const override;
    std::vector<Entry> getAllBlobsSortedById() const override;

    BlobId storeBlob (Entry&& entry) override;

    void deleteBlob (BlobType type, const ErpVector& name) override;

    std::vector<bool> hasValidBlobsOfType (std::vector<BlobType>&& blobTypes) const override;

    class Transaction
    {
    public:
        explicit Transaction(std::unique_ptr<pqxx::transaction_base>&& transaction);

        void commit();
        pqxx::transaction_base* operator->();

    private:
        std::unique_ptr<pqxx::transaction_base> mWork;
    };
    Transaction createTransaction() const;
    void recreateConnection() override;

private:
    mutable PostgresConnection mConnection;

    static BlobDatabase::Entry convertEntry (const pqxx::row& dbEntry);

    [[noreturn]]
    static void processStoreBlobException();
};

#endif

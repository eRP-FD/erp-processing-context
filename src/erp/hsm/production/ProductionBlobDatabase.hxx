#ifndef ERP_PROCESSING_CONTEXT_PRODUCTIONBLOBDATABASE_HXX
#define ERP_PROCESSING_CONTEXT_PRODUCTIONBLOBDATABASE_HXX

#include "erp/enrolment/EnrolmentData.hxx"

#include "erp/database/PostgresConnection.hxx"
#include "erp/hsm/BlobDatabase.hxx"

#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include <pqxx/transaction>

namespace pqxx {class connection;}


/**

 */
class ProductionBlobDatabase : public BlobDatabase
{
public:
    ProductionBlobDatabase (void);
    explicit ProductionBlobDatabase (const std::string& connectionString);

    virtual Entry getBlob (
        BlobType type,
        BlobId id) const override;
    virtual std::vector<Entry> getAllBlobsSortedById (void) const override;

    virtual BlobId storeBlob (Entry&& entry) override;

    virtual void deleteBlob (BlobType type, const ErpVector& name) override;

    virtual std::vector<bool> hasValidBlobsOfType (std::vector<BlobType>&& blobTypes) const override;

    class Transaction
    {
    public:
        explicit Transaction (pqxx::connection& connection);

        void commit (void);
        pqxx::work* operator-> (void); // This exists for historical reasons.

    private:
        pqxx::work mWork;
    };
    Transaction createTransaction(void) const;

private:
    mutable PostgresConnection mConnection;

    static BlobDatabase::Entry convertEntry (const pqxx::row& dbEntry);

    [[noreturn]]
    void processStoreBlobException (void);
};

#endif

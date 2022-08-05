/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_POSTGRESDATABASETEST_HXX
#define ERP_PROCESSING_CONTEXT_POSTGRESDATABASETEST_HXX

#include "erp/compression/ZStd.hxx"
#include "erp/crypto/Jwt.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/DatabaseModel.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test/erp/model/CommunicationTest.hxx"
#include "test/util/TestConfiguration.hxx"
#include "test_config.h"
#include "test/util/JwtBuilder.hxx"


#include <boost/format.hpp>
#include <gtest/gtest.h>
#include <pqxx/pqxx>


class PostgresDatabaseTest : public ::testing::Test
{
public:

    PostgresDatabaseTest();

    virtual void cleanup() {}

    virtual void verifyDatabaseIsTidy() {}

    void SetUp() override
    {
        cleanup();
    }

    void TearDown() override
    {
        EXPECT_FALSE(databaseNeedsCommit())
                << "Database transaction not committed at the end of the test";
        if (mDatabase)
        {
            mDatabase.reset();
        }
        cleanup();
        if (mConnection)
        {
            mConnection.reset();
        }
    }

    Database& database ()
    {
        if ( ! mDatabase || mDatabase->getBackend().isCommitted())
        {
            Expect(usePostgres(), "database support is disabled, database should not be used");
            mDatabase = std::make_unique<DatabaseFrontend>(
                            std::make_unique<PostgresBackend>(), *mHsmPool, *mKeyDerivation);
        }
        return *mDatabase;
    }

    const DataBaseCodec& getDBCodec()
    {
        static auto theCompressionInstance =
            std::make_shared<ZStd>(Configuration::instance().getStringValue(ConfigurationKey::ZSTD_DICTIONARY_DIR));
        static DataBaseCodec codec(theCompressionInstance);
        return codec;
    }

    static bool usePostgres()
    {
        return TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false);
    }

    std::shared_ptr<BlobCache> blobCache() { return mBlobCache; }


protected:
    class Query {
    public:
        std::string const name;
        std::string const query;
    };

    pqxx::connection& getConnection()
    {
        Expect(usePostgres(), "database support is disabled, database should not be used");
        if (!mConnection)
        {
            mConnection = std::make_unique<pqxx::connection>(PostgresBackend::defaultConnectString());
        }
        return *mConnection;
    }

    bool databaseNeedsCommit()
    {
        return mDatabase && not mDatabase->getBackend().isCommitted();
    }

    pqxx::work createTransaction()
    {
        Expect3(!databaseNeedsCommit(), "previous transaction not completed", std::logic_error);
        return pqxx::work{getConnection()};
    }

    void prepare(const Query& query)
    {
        getConnection().prepare(query.name, query.query);
    }

    template <typename ContainerT>
    std::string toHex(const ContainerT& container)
    {
        const char* const data = reinterpret_cast<const char*>(container.data());
        auto size = sizeof(*container.data()) * container.size();
        return ByteHelper::toHex(gsl::span<const char>{data, size});
    }

    void clearTables()
    {
        auto deleteTxn = createTransaction();
        deleteTxn.exec("DELETE FROM erp.communication");
        deleteTxn.exec("DELETE FROM erp.task");
        deleteTxn.exec("DELETE FROM erp.task_169");
        deleteTxn.exec("DELETE FROM erp.task_200");
        deleteTxn.exec("DELETE FROM erp.charge_item");
        deleteTxn.exec("DELETE FROM erp.auditevent");
        deleteTxn.commit();
    }

    // Please note that for a doctor or a pharmacy the "JwtBuilder.makeJwt<>" methods are
    // retrieving the names from json files ("claims_arzt.json" or "claims_apotheke.json").
    // For our tests one doctor and one pharmacist is sufficient to handle most test cases.
    // Names of doctors and pharmacies may have up to 256 characters.
    // In contrary several tests need different insurants so here a set of predefined
    // insurant names is provided. The KVNRs of insurants is limited to 10 characters.
    static constexpr const char* InsurantA = "X000000001";
    static constexpr const char* InsurantB = "X000000002";
    static constexpr const char* InsurantC = "X000000003";
    static constexpr const char* InsurantX = "X000000004";

    // KVNRs for insurants as defined in "resources/test/EndpointHandlerTest/task<Nr>.json".
    // In "task[1,2,8]" the KVNR "X123456789" is defined.
    // In "task[4,5,6,8]" the KVNR "X234567890" is defined.
    // In "task3.json" no KVNR is defined.
    static constexpr const char* InsurantTask128 = "X123456789";
    static constexpr const char* InsurantTask4567 = "X234567890";


    KeyDerivation& getKeyDerivation()
    {
        return *mKeyDerivation;
    }


    void cleanKvnr(std::string_view kvnr, const std::string& taskTableName)
    {
        auto&& txn = createTransaction();
        auto kvnr_hashed = mKeyDerivation->hashKvnr(kvnr);
        txn.exec_params("DELETE FROM " + taskTableName + " WHERE kvnr=$1", kvnr_hashed.binarystring());
        txn.exec_params("DELETE FROM erp.auditevent WHERE kvnr_hashed=$1", kvnr_hashed.binarystring());
        txn.commit();
    }

    shared_EVP_PKEY mIdpPrivateKey;
    JwtBuilder      mJwtBuilder;
    std::string     mPharmacy;

private:
    std::unique_ptr<pqxx::connection> mConnection;
    std::unique_ptr<DatabaseFrontend> mDatabase;
    std::shared_ptr<BlobCache> mBlobCache;
    std::unique_ptr<HsmPool> mHsmPool;
    std::unique_ptr<KeyDerivation> mKeyDerivation;
};

#endif

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_POSTGRESDATABASETEST_HXX
#define ERP_PROCESSING_CONTEXT_POSTGRESDATABASETEST_HXX

#include "shared/compression/ZStd.hxx"
#include "shared/crypto/Jwt.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "shared/database/DatabaseModel.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/model/TelematikId.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/DurationConsumer.hxx"
#include "shared/util/Expect.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test_config.h"
#include "test/erp/model/CommunicationTest.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/TestConfiguration.hxx"

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

    std::string taskTableName(model::PrescriptionType type) const
    {
        switch (type)
        {
            case model::PrescriptionType::apothekenpflichigeArzneimittel:
                return "erp.task";
            case model::PrescriptionType::direkteZuweisung:
                return "erp.task_169";
            case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
                return "erp.task_200";
            case model::PrescriptionType::direkteZuweisungPkv:
                return "erp.task_209";
        }
        Fail("invalid prescription type: " + std::to_string(uintmax_t(type)));
    }

    pqxx::connection& getConnection()
    {
        Expect(usePostgres(), "database support is disabled, database should not be used");
        if (!mConnection)
        {
            mConnection = std::make_unique<pqxx::connection>(PostgresConnection::defaultConnectString());
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
        deleteTxn.exec("DELETE FROM erp.task_209");
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
    static constexpr const char* InsurantA = "X000000012";
    static constexpr const char* InsurantB = "X000000024";
    static constexpr const char* InsurantC = "X000000036";

    KeyDerivation& getKeyDerivation()
    {
        return *mKeyDerivation;
    }


    void cleanKvnr(const model::Kvnr& kvnr, const std::string& taskTableName)
    {
        auto&& txn = createTransaction();
        auto kvnr_hashed = mKeyDerivation->hashKvnr(kvnr);
        txn.exec_params("DELETE FROM " + taskTableName + " WHERE kvnr=$1", kvnr_hashed.binarystring());
        txn.exec_params("DELETE FROM erp.auditevent WHERE kvnr_hashed=$1", kvnr_hashed.binarystring());
        txn.commit();
    }

    shared_EVP_PKEY mIdpPrivateKey;
    JwtBuilder      mJwtBuilder;
    model::TelematikId mPharmacy;

private:
    std::unique_ptr<pqxx::connection> mConnection;
    std::unique_ptr<DatabaseFrontend> mDatabase;
    std::shared_ptr<BlobCache> mBlobCache;
    std::unique_ptr<HsmPool> mHsmPool;
    std::unique_ptr<KeyDerivation> mKeyDerivation;
    std::unique_ptr<DurationConsumerGuard> mDurationConsumerGuard;
};

#endif

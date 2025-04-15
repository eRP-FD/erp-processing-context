/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/erp/database/PostgresDatabaseTest.hxx"
#include "shared/util/Version.hxx"


class PostgresDatabaseSchemaVersionTest : public PostgresDatabaseTest
{
public:
    void SetUp() override
    {
        if (usePostgres())
        {
            prepare(mIouSchemaVersion);
            prepare(mDeleteSchemaVersion);
            pqxx::result result;
            auto &&txn = createTransaction();
            ASSERT_NO_THROW(result = txn.exec_params("SELECT value FROM erp.config WHERE parameter = 'schema_version'"));
            ASSERT_TRUE(result.size() <= 1);
            if(!result.empty())
            {
                ASSERT_NO_THROW(mCurrentSchemaVersion = result[0].at(0).as<std::string>());
            }
            txn.commit();
        }
    }

    void TearDown() override
    {
        if (usePostgres())
        {
            if (mCurrentSchemaVersion.has_value())
            {
                insertOrUpdateSchemaVersion(mCurrentSchemaVersion.value());
            }
            else
            {
                deleteSchemaVersion();
            }
        }
    }

    void insertOrUpdateSchemaVersion(std::string_view version)
    {
        pqxx::result result;
        auto &&txn = createTransaction();
        ASSERT_NO_THROW(result = txn.exec_prepared(mIouSchemaVersion.name, version));
        ASSERT_TRUE(result.empty());
        txn.commit();
    }

    void deleteSchemaVersion()
    {
        pqxx::result result;
        auto &&txn = createTransaction();
        ASSERT_NO_THROW(result = txn.exec_prepared(mDeleteSchemaVersion.name));
        ASSERT_TRUE(result.empty());
        txn.commit();
    }

private:
    std::optional<std::string> mCurrentSchemaVersion;

    const Query mIouSchemaVersion{"iouSchemaVersion",
                                  "INSERT INTO erp.config (parameter, value) VALUES ('schema_version', $1) "
                                  "ON CONFLICT(parameter) "
                                  "DO UPDATE SET value = $1"};
    const Query mDeleteSchemaVersion{"deleteSchemaVersion",
                                     "DELETE FROM erp.config WHERE parameter = 'schema_version'"};
};

TEST_F(PostgresDatabaseSchemaVersionTest, validateCurrentVersion)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    ASSERT_EQ(Version(database().retrieveSchemaVersion()), Version(Database::expectedSchemaVersion));
    database().commitTransaction();
}

TEST_F(PostgresDatabaseSchemaVersionTest, readVersion)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    ASSERT_NO_FATAL_FAILURE(insertOrUpdateSchemaVersion("42"));
    EXPECT_EQ(database().retrieveSchemaVersion(), "42");
    database().commitTransaction();
    ASSERT_NO_FATAL_FAILURE(insertOrUpdateSchemaVersion(Database::expectedSchemaVersion));
    EXPECT_EQ(database().retrieveSchemaVersion(), Database::expectedSchemaVersion);
    database().commitTransaction();
}

TEST_F(PostgresDatabaseSchemaVersionTest, noVersion)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    ASSERT_NO_FATAL_FAILURE(deleteSchemaVersion());
    EXPECT_THROW(database().retrieveSchemaVersion(), std::runtime_error);
    database().commitTransaction();
}
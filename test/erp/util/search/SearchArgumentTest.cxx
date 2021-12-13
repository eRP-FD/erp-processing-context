/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/database/PostgresBackend.hxx"

#include "erp/database/Database.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "test/erp/model/CommunicationTest.hxx"
#include "test/util/JsonTestUtils.hxx"
#include "test/util/ServerTestBase.hxx"
#include "test_config.h"

#include <pqxx/transaction>


/**
 * Escaping values for use in a PostgreSQL WHERE expression requires a live Postgres connection.
 * That is the reason for using ServerTestBase as a base class.
 * And as with this the infrastructure for more interesting tests against a Postgres database are already there,
 * there are tests that do just that. They use the erp.communication table because
 * - it allows the tests to run without having to create a test-only table
 * - the search on Communication objects is more complex than on Task objects (two date parameters and two text parameters).
 *
 * Re naming of this class. Usually test classes are named after the primary test subject. In this case that would be UrlArguments.
 * But as UrlArguments supports different types of arguments like searching, sorting and paging, it makes sense to
 * separate these. Therefore the name is based on the secondary test subject, SearchArgument.
 */
class SearchArgumentTest : public ServerTestBase
{
public:
    static constexpr const char* sender = "KVNRsendrA";
    static constexpr const char* recipient = "X-234567890";

    void SetUp (void) override
    {
        ServerTestBase::SetUp();

        removeTestData();

        auto database = createDatabase();

        // Create a task object that can be referenced from communication objects.
        std::string dataPath = std::string(TEST_DATA_DIR) + "/EndpointHandlerTest";
        std::string jsonString = FileHelper::readFileAsString(dataPath + "/task1.json");
        model::Task task = model::Task::fromJsonNoValidation(jsonString);
        mTaskId.emplace(database->storeTask(task));

        // Create a communication object.
        model::Communication communication = model::Communication::fromJsonNoValidation(
            CommunicationJsonStringBuilder(model::Communication::MessageType::Representative)
                .setSender(ActorRole::Representative, sender)
                .setRecipient(ActorRole::Representative, recipient)
                .setTimeSent("2021-09-08T12:34:56.123+00:00")
                .setPrescriptionId(mTaskId.value().toString())
                .createJsonString());
        mCommunicationId = database->insertCommunication(communication);
        mCommunicationSender = communication.sender().value();
        communication.setId(mCommunicationId.value());
        database->commitTransaction();
    }

    void TearDown (void) override
    {
        auto database = createDatabase();
        if (mCommunicationId.has_value())
        {
            database->deleteCommunication(mCommunicationId.value(), mCommunicationSender);
            database->commitTransaction();
        }
        if (mTaskId.has_value())
        {
            if (mHasPostgresSupport)
            {
                auto deleteTxn = createTransaction();
                deleteTxn.exec("DELETE FROM erp.task");
                deleteTxn.commit();
            }
            // else the mock database is not expected to be persistent and therefore does not have to be cleaned up.
        }
    }

    void testSyntax (const std::string& key, const std::string& value, const std::string& expectedResult)
    {
        // Postgres quoting functionality requires a postgres connection.
        // That is why this test is disabled when the DB is not available.
        if ( ! mHasPostgresSupport)
            GTEST_SKIP();

        auto search = UrlArguments(
            {{"sent", "erp.timestamp_from_suuid(id)",     SearchParameter::Type::Date},
             {"received",  SearchParameter::Type::Date},
             {"sender",    SearchParameter::Type::HashedIdentity},
             {"recipient", SearchParameter::Type::HashedIdentity}
            });
        Header header;
        ServerRequest request(std::move(header));
        request.setQueryParameters({
            {key,                      value},
            {"nonsense:to-be-ignored", "also to be ignored"}
        });
        search.parse(request, mServer->serviceContext()->getKeyDerivation());

        ASSERT_EQ(search.getSqlWhereExpression(getConnection()), expectedResult);
    }


    void testSql(const std::string &key, const std::string &value, const size_t expectedResultCount)
    {
        if ( ! mHasPostgresSupport)
            GTEST_SKIP();

        auto search = UrlArguments(
            {{"sent", "erp.timestamp_from_suuid(id)",     SearchParameter::Type::Date},
             {"received",  SearchParameter::Type::Date},
             {"sender",    SearchParameter::Type::HashedIdentity},
             {"recipient", SearchParameter::Type::HashedIdentity}
            });
        Header header;
        ServerRequest request(std::move(header));
        request.setQueryParameters({
            {key,                      value},
            {"nonsense:to-be-ignored", "also to be ignored"}
        });
        search.parse(request, mServer->serviceContext()->getKeyDerivation());

        const std::string whereExpression = search.getSqlWhereExpression(getConnection());
        const std::string query =
            "SELECT id FROM erp.communication WHERE sender = '\\x" + hashedHex(sender) + "' AND " + whereExpression;
        pqxx::work transaction(getConnection());
        const auto result = transaction.exec(query);
        ASSERT_EQ(result.size(), expectedResultCount)
                            << "expected result count " << expectedResultCount << " but got " << result.size()
                            << " for " << key << " " << value;
    }


    void testFailure(const std::string &key, const std::string &value)
    {
        auto search = UrlArguments(
            {{"sent", "erp.timestamp_from_suuid(id)",     SearchParameter::Type::Date},
             {"received",  SearchParameter::Type::Date},
             {"sender",    SearchParameter::Type::HashedIdentity},
             {"recipient", SearchParameter::Type::HashedIdentity}
            });
        Header header;
        ServerRequest request(std::move(header));
        request.setQueryParameters({
            {key,                      value},
            {"nonsense:to-be-ignored", "also to be ignored"}
        });

        ASSERT_ANY_THROW(search.parse(request, mServer->serviceContext()->getKeyDerivation()));
    }
    std::string hashedHex(const std::string_view& id)
    {
        return mServer->serviceContext()->getKeyDerivation().hashIdentity(id).toHex();
    }


private:
    std::optional<Uuid> mCommunicationId;
    std::optional<model::PrescriptionId> mTaskId;
    std::string mCommunicationSender;

    static bool mIsIntialCleanupRequested; // static so that database cleanup is only executed once before the first test runs.

    void removeTestData (void)
    {
        if (mHasPostgresSupport && mIsIntialCleanupRequested)
        {
            mIsIntialCleanupRequested = false;

            const std::string query = "DELETE FROM erp.communication WHERE sender = '\\x" + hashedHex(sender) + "'";
            auto connection = std::make_unique<pqxx::connection>(PostgresBackend::defaultConnectString());
            pqxx::work transaction (*connection);
            const auto result = transaction.exec(query);
            transaction.commit();
        }
    }
};
bool SearchArgumentTest::mIsIntialCleanupRequested = true;


TEST_F(SearchArgumentTest, getSelfLinkPathParameters)
{
    auto search = UrlArguments(
        {{"sent", "erp.timestamp_from_suuid(id)",     SearchParameter::Type::Date},
         {"received",  SearchParameter::Type::Date},
         {"sender",    SearchParameter::Type::HashedIdentity},
         {"recipient", SearchParameter::Type::HashedIdentity}
        });
    Header header;
    ServerRequest request(std::move(header));
    request.setQueryParameters({
        {"sent",          "lt2021-09-08"},
        {"recipient",     "KVNR123456"},
        {"sent",          "lt2021-09-08T23:32:58+12:34"},
        {"to-be-ignored", "also to be ignored"}
    });
    search.parse(request, mServer->serviceContext()->getKeyDerivation());

    // Expectations:
    // - the first 'sent' is not expanded to include a time.
    // - the 'recipient' is prefixed with the default "eq:".
    // - the second 'sent' timestamp is normalized to UTC.
    // - the 'to-be-ignored' argument is ignored and not included in the self link.
    ASSERT_EQ(search.getLinkPathArguments(model::Link::Type::Self),
        "?sent=lt2021-09-08&recipient=KVNR123456&sent=lt2021-09-08T10:58:58+00:00");
}


TEST_F(SearchArgumentTest, multipleParameters)
{
    if ( ! mHasPostgresSupport)
        GTEST_SKIP();

    auto search = UrlArguments(
        {{"sent", "erp.timestamp_from_suuid(id)",     SearchParameter::Type::Date},
         {"received",  SearchParameter::Type::Date},
         {"sender",    SearchParameter::Type::HashedIdentity},
         {"recipient", SearchParameter::Type::HashedIdentity}
        });
    Header header;
    ServerRequest request(std::move(header));
    request.setQueryParameters({
        {"sent",          "gt2021-09-08"},
        {"recipient",     "KVNR123456"},
        {"sent",          "gt2022-01-08T23:32:58+12:34"},
        {"to-be-ignored", "also to be ignored"}
    });
    search.parse(request, mServer->serviceContext()->getKeyDerivation());

    // Expectations:
    // - for the first 'sent' the end of the implicit, day-long, range is used
    // - the 'recipient' is prefixed with the default "eq:".
    // - the second 'sent' timestamp is normalized to UTC.
    // - the 'to-be-ignored' argument is ignored and not included in the self link.
    ASSERT_EQ(search.getSqlWhereExpression(getConnection()),
        "(erp.timestamp_from_suuid(id) >= '2021-09-09T00:00:00+00:00') AND (recipient = '\\x"
        + hashedHex("KVNR123456") + "') AND (erp.timestamp_from_suuid(id) >= '2022-01-08T10:58:59+00:00')");
}


TEST_F(SearchArgumentTest, multipleParametersMultipleValues)
{
    if (!mHasPostgresSupport)
        GTEST_SKIP();

    auto search = UrlArguments(
        {{"sent", "erp.timestamp_from_suuid(id)",     SearchParameter::Type::Date},
         {"received",  SearchParameter::Type::Date},
         {"sender",    SearchParameter::Type::HashedIdentity},
         {"recipient", SearchParameter::Type::HashedIdentity}
        });
    Header header;
    ServerRequest request(std::move(header));
    request.setQueryParameters({
        {"recipient",     "KVNR123456,KVNR654321"},
        {"sender",        "KVNR123456,KVNR654321"},
        {"sent",          "2021-09-08,2021-09-01"},
        {"received",      "gt2022-01-08,2021-09-01"} // doesn't make sense but its not a failure
        });
    search.parse(request, mServer->serviceContext()->getKeyDerivation());

    // Expectations:
    // - the 'recipients' are prefixed with the default "eq:" and ored.
    // - the 'senders' are prefixed with the default "eq:" and ored.
    // - the 'sent' timestamps are normalized to UTC and ored.
    // - the 'received' timestamps  are normalized to UTC and ored.
    ASSERT_EQ(search.getSqlWhereExpression(getConnection()),
        "((recipient = '\\x" + hashedHex("KVNR123456") + "') OR (recipient = '\\x" + hashedHex("KVNR654321") + "'))"
        + " AND ((sender = '\\x" + hashedHex("KVNR123456") + "') OR (sender = '\\x" + hashedHex("KVNR654321") + "'))"
        + " AND ((('2021-09-08T00:00:00+00:00' <= erp.timestamp_from_suuid(id)) AND (erp.timestamp_from_suuid(id) < '2021-09-09T00:00:00+00:00'))"
        + " OR (('2021-09-01T00:00:00+00:00' <= erp.timestamp_from_suuid(id)) AND (erp.timestamp_from_suuid(id) < '2021-09-02T00:00:00+00:00')))"
        + " AND ((received >= '2022-01-09T00:00:00+00:00') OR (received >= '2021-09-02T00:00:00+00:00'))");
}


TEST_F(SearchArgumentTest, multipleParametersMultipleValuesWithFailure)
{
    if (!mHasPostgresSupport)
        GTEST_SKIP();

    auto search = UrlArguments(
        { {"sent", "erp.timestamp_from_suuid(id)",     SearchParameter::Type::Date},
         {"received",  SearchParameter::Type::Date},
         {"sender",    SearchParameter::Type::HashedIdentity},
         {"recipient", SearchParameter::Type::HashedIdentity}
        });
    Header header;
    ServerRequest request(std::move(header));
    request.setQueryParameters({
        {"recipient",     "KVNR123456,KVNR654321"},
        {"sender",        "KVNR123456,KVNR654321"},
        {"sent",          "gt2021-09-08,lt2021-09-01"} // changing prefix is not supported
        });
    ASSERT_THROW(search.parse(request, mServer->serviceContext()->getKeyDerivation()), ErpException);
}


TEST_F(SearchArgumentTest, eq_date_syntax)
{
    testSyntax("received", "eq2021",                          "(('2021-01-01T00:00:00+00:00' <= received) AND (received < '2022-01-01T00:00:00+00:00'))");
    testSyntax("received", "eq2021-09",                       "(('2021-09-01T00:00:00+00:00' <= received) AND (received < '2021-10-01T00:00:00+00:00'))");
    testSyntax("received", "eq2021-09-08",                    "(('2021-09-08T00:00:00+00:00' <= received) AND (received < '2021-09-09T00:00:00+00:00'))");
    testSyntax("received", "eq2021-09-08T23:32:58+00:00",     "(('2021-09-08T23:32:58+00:00' <= received) AND (received < '2021-09-08T23:32:59+00:00'))");
    testSyntax("received", "eq2021-09-08T23:32:58+12:34",     "(('2021-09-08T10:58:58+00:00' <= received) AND (received < '2021-09-08T10:58:59+00:00'))");
    testSyntax("received", "eq2021-09-07T23:32:59Z",          "(('2021-09-07T23:32:59+00:00' <= received) AND (received < '2021-09-07T23:33:00+00:00'))");
    testSyntax("received", "eq2021-09-07T23:32:59",           "(('2021-09-07T23:32:59+00:00' <= received) AND (received < '2021-09-07T23:33:00+00:00'))");
    testSyntax("received", "eq2021-09-07T20:59",              "(('2021-09-07T20:59:00+00:00' <= received) AND (received < '2021-09-07T21:00:00+00:00'))");
    testSyntax("received", "eq2021-09-07T20:59Z",              "(('2021-09-07T20:59:00+00:00' <= received) AND (received < '2021-09-07T21:00:00+00:00'))");
    testSyntax("received", "eq2021-09-07T20:59+02:00",        "(('2021-09-07T18:59:00+00:00' <= received) AND (received < '2021-09-07T19:00:00+00:00'))");
}


TEST_F(SearchArgumentTest, eq_date_sql)
{
    // Success with B <= T < E
    testSql("sent", "eq2021",                          1);
    testSql("sent", "eq2021-09",                       1);
    testSql("sent", "eq2021-09-08",                    1);
    testSql("sent", "eq2021-09-08T12:34:56+00:00",     1);
    testSql("sent", "eq2021-09-08T12:34:56",           1);
    testSql("sent", "eq2021-09-08T12:34Z",             1);
    testSql("sent", "eq2021-09-08T12:34",              1);

    // Fail for T < B
    testSql("sent", "eq2022",                          0);
    testSql("sent", "eq2021-10",                       0);
    testSql("sent", "eq2021-09-09",                    0);
    testSql("sent", "eq2021-09-08T12:34:57+00:00",     0);
    testSql("sent", "eq2021-09-08T12:35+00:00",        0);
    testSql("sent", "eq2021-09-08T12:35",              0);

    // Fail for T >= E
    testSql("sent", "eq2011",                          0);
    testSql("sent", "eq2021-08",                       0);
    testSql("sent", "eq2021-09-07",                    0);
    testSql("sent", "eq2021-09-08T12:34:55+00:00",     0);
    testSql("sent", "eq2021-09-08T12:33+00:00",        0);
}


TEST_F(SearchArgumentTest, ne_date_syntax)
{
    testSyntax("received", "ne2021",                          "(('2021-01-01T00:00:00+00:00' > received) OR (received >= '2022-01-01T00:00:00+00:00'))");
    testSyntax("received", "ne2021-09",                       "(('2021-09-01T00:00:00+00:00' > received) OR (received >= '2021-10-01T00:00:00+00:00'))");
    testSyntax("received", "ne2021-09-08",                    "(('2021-09-08T00:00:00+00:00' > received) OR (received >= '2021-09-09T00:00:00+00:00'))");
    testSyntax("received", "ne2021-09-08T23:32:58+00:00",     "(('2021-09-08T23:32:58+00:00' > received) OR (received >= '2021-09-08T23:32:59+00:00'))");
    testSyntax("received", "ne2021-09-07T23:32:59Z",          "(('2021-09-07T23:32:59+00:00' > received) OR (received >= '2021-09-07T23:33:00+00:00'))");
    testSyntax("received", "ne2021-09-07T23:32:59",           "(('2021-09-07T23:32:59+00:00' > received) OR (received >= '2021-09-07T23:33:00+00:00'))");
    testSyntax("received", "ne2021-09-07T20:59",              "(('2021-09-07T20:59:00+00:00' > received) OR (received >= '2021-09-07T21:00:00+00:00'))");
    testSyntax("received", "ne2021-09-07T20:59Z",             "(('2021-09-07T20:59:00+00:00' > received) OR (received >= '2021-09-07T21:00:00+00:00'))");
    testSyntax("received", "ne2021-09-07T20:59+02:00",        "(('2021-09-07T18:59:00+00:00' > received) OR (received >= '2021-09-07T19:00:00+00:00'))");
}


TEST_F(SearchArgumentTest, ne_date_sql)
{
    // Success with T < B
    testSql("sent", "ne2022",                          1);
    testSql("sent", "ne2021-10",                       1);
    testSql("sent", "ne2021-09-09",                    1);
    testSql("sent", "ne2021-09-08T12:34:57+00:00",     1);
    testSql("sent", "ne2021-09-08T12:35+00:00",        1);
    testSql("sent", "ne2021-09-08T12:35",              1);

    // Success with T >= E
    testSql("sent", "ne2011",                          1);
    testSql("sent", "ne2021-08",                       1);
    testSql("sent", "ne2021-09-07",                    1);
    testSql("sent", "ne2021-09-08T12:34:55+00:00",     1);
    testSql("sent", "ne2021-09-08T12:33+00:00",        1);

    // Fail for B <= T < E
    testSql("sent", "ne2021",                          0);
    testSql("sent", "ne2021-09",                       0);
    testSql("sent", "ne2021-09-08",                    0);
    testSql("sent", "ne2021-09-08T12:34:56+00:00",     0);
    testSql("sent", "ne2021-09-08T12:34:56",           0);
    testSql("sent", "ne2021-09-08T12:34Z",             0);
    testSql("sent", "ne2021-09-08T12:34",              0);
}


TEST_F(SearchArgumentTest, gt_date_syntax)
{
    testSyntax("received", "gt2021",                          "(received >= '2022-01-01T00:00:00+00:00')");
    testSyntax("received", "gt2021-09",                       "(received >= '2021-10-01T00:00:00+00:00')");
    testSyntax("received", "gt2021-09-08",                    "(received >= '2021-09-09T00:00:00+00:00')");
    testSyntax("received", "gt2021-09-08T23:32:58+00:00",     "(received >= '2021-09-08T23:32:59+00:00')");
    testSyntax("received", "gt2021-09-07T23:32:59Z",          "(received >= '2021-09-07T23:33:00+00:00')");
    testSyntax("received", "gt2021-11-11T12:34:22",           "(received >= '2021-11-11T12:34:23+00:00')");
    testSyntax("received", "gt2021-09-07T20:59",              "(received >= '2021-09-07T21:00:00+00:00')");
    testSyntax("received", "gt2021-09-07T20:59Z",             "(received >= '2021-09-07T21:00:00+00:00')");
    testSyntax("received", "gt2021-09-07T20:59+02:00",        "(received >= '2021-09-07T19:00:00+00:00')");
}


TEST_F(SearchArgumentTest, gt_date_sql)
{
    // Success with T >= E
    testSql("sent", "gt2011",                          1);
    testSql("sent", "gt2021-08",                       1);
    testSql("sent", "gt2021-09-07",                    1);
    testSql("sent", "gt2021-09-08T12:33:55+00:00",     1);
    testSql("sent", "gt2021-09-08T12:33+00:00",        1);

    // Fail for T < E
    testSql("sent", "gt2021",                          0);
    testSql("sent", "gt2021-09",                       0);
    testSql("sent", "gt2021-09-08",                    0);
    testSql("sent", "gt2021-09-08T12:34:56+00:00",     0);
    testSql("sent", "gt2021-09-08T12:34+00:00",        0);
    testSql("sent", "gt2021-09-08T12:34",              0);
}


TEST_F(SearchArgumentTest, lt_date_syntax)
{
    testSyntax("received", "lt2021",                          "(received < '2021-01-01T00:00:00+00:00')");
    testSyntax("received", "lt2021-09",                       "(received < '2021-09-01T00:00:00+00:00')");
    testSyntax("received", "lt2021-09-08",                    "(received < '2021-09-08T00:00:00+00:00')");
    testSyntax("received", "lt2021-09-08T23:32:58+00:00",     "(received < '2021-09-08T23:32:58+00:00')");
    testSyntax("received", "lt2021-09-07T23:32:59Z",          "(received < '2021-09-07T23:32:59+00:00')");
    testSyntax("received", "lt2021-11-11T12:34:22",           "(received < '2021-11-11T12:34:22+00:00')");
    testSyntax("received", "lt2021-09-07T20:59",              "(received < '2021-09-07T20:59:00+00:00')");
    testSyntax("received", "lt2021-09-07T20:59Z",             "(received < '2021-09-07T20:59:00+00:00')");
    testSyntax("received", "lt2021-09-07T20:59+02:00",        "(received < '2021-09-07T18:59:00+00:00')");
}


TEST_F(SearchArgumentTest, lt_date_sql)
{
    // Success with T < B
    testSql("sent", "lt2022", 1);
    testSql("sent", "lt2021-10", 1);
    testSql("sent", "lt2021-09-09", 1);
    testSql("sent", "lt2021-09-08T23:34:57+00:00", 1);
    testSql("sent", "lt2021-09-08T12:35+00:00", 1);
    testSql("sent", "lt2021-09-08T12:35", 1);

    // Fail for T >= B
    testSql("sent", "lt2021",                          0);
    testSql("sent", "lt2021-09",                       0);
    testSql("sent", "lt2021-09-08",                    0);
    testSql("sent", "lt2021-09-08T12:34:56+00:00",     0);
    testSql("sent", "lt2021-09-08T12:34+00:00",        0);
    testSql("sent", "lt2021-09-08T12:34",              0);
}


TEST_F(SearchArgumentTest, ge_date_syntax)
{
    testSyntax("received", "ge2021",                          "(received >= '2021-01-01T00:00:00+00:00')");
    testSyntax("received", "ge2021-09",                       "(received >= '2021-09-01T00:00:00+00:00')");
    testSyntax("received", "ge2021-09-08",                    "(received >= '2021-09-08T00:00:00+00:00')");
    testSyntax("received", "ge2021-09-08T23:32:58+00:00",     "(received >= '2021-09-08T23:32:58+00:00')");
    testSyntax("received", "ge2021-09-07T23:32:59Z",          "(received >= '2021-09-07T23:32:59+00:00')");
    testSyntax("received", "ge2021-11-11T12:34:22",           "(received >= '2021-11-11T12:34:22+00:00')");
    testSyntax("received", "ge2021-09-07T20:59",              "(received >= '2021-09-07T20:59:00+00:00')");
    testSyntax("received", "ge2021-09-07T20:59Z",             "(received >= '2021-09-07T20:59:00+00:00')");
    testSyntax("received", "ge2021-09-07T20:59+02:00",        "(received >= '2021-09-07T18:59:00+00:00')");
}


TEST_F(SearchArgumentTest, ge_date_sql)
{
    // Success with T >= B
    testSql("sent", "ge2021",                          1);
    testSql("sent", "ge2021-09",                       1);
    testSql("sent", "ge2021-09-08",                    1);
    testSql("sent", "ge2021-09-08T12:33:56+00:00",     1);
    testSql("sent", "ge2021-09-08T12:33+00:00",        1);
    testSql("sent", "ge2021-09-08T12:33",              1);

    // Fail for T < B
    testSql("sent", "ge2022",                          0);
    testSql("sent", "ge2021-10",                       0);
    testSql("sent", "ge2021-09-09",                    0);
    testSql("sent", "ge2021-09-08T12:34:57+00:00",     0);
    testSql("sent", "ge2021-09-08T12:35+00:00",        0);
    testSql("sent", "ge2021-09-08T12:35",              0);
}


TEST_F(SearchArgumentTest, le_date_syntax)
{
    testSyntax("received", "le2021",                          "(received < '2022-01-01T00:00:00+00:00')");
    testSyntax("received", "le2021-09",                       "(received < '2021-10-01T00:00:00+00:00')");
    testSyntax("received", "le2021-09-08",                    "(received < '2021-09-09T00:00:00+00:00')");
    testSyntax("received", "le2021-09-08T23:32:58+00:00",     "(received < '2021-09-08T23:32:59+00:00')");
    testSyntax("received", "le2021-09-07T23:32:59Z",          "(received < '2021-09-07T23:33:00+00:00')");
    testSyntax("received", "le2021-11-11T12:34:22",           "(received < '2021-11-11T12:34:23+00:00')");
    testSyntax("received", "le2021-09-07T20:59",              "(received < '2021-09-07T21:00:00+00:00')");
    testSyntax("received", "le2021-09-07T20:59Z",             "(received < '2021-09-07T21:00:00+00:00')");
    testSyntax("received", "le2021-09-07T20:59+02:00",        "(received < '2021-09-07T19:00:00+00:00')");
}


TEST_F(SearchArgumentTest, le_date_sql)
{
    // Success with T < E
    testSql("sent", "le2021",                          1);
    testSql("sent", "le2021-09",                       1);
    testSql("sent", "le2021-09-08",                    1);
    testSql("sent", "le2021-09-08T12:34:56+00:00",     1);
    testSql("sent", "le2021-09-08T12:34+00:00",        1);
    testSql("sent", "le2021-09-08T12:34",              1);

    // Fail for T >= E
    testSql("sent", "le2011",                          0);
    testSql("sent", "le2021-08",                       0);
    testSql("sent", "le2021-09-07",                    0);
    testSql("sent", "le2021-09-08T12:34:55+00:00",     0);
    testSql("sent", "le2021-09-08T12:33+00:00",        0);
    testSql("sent", "le2021-09-08T12:33",              0);
}


/**
 * Due to gt and sa being equivalent when target values are time points, the sa_date_syntax test is a copy of the gt_date_syntax test.
 */
TEST_F(SearchArgumentTest, sa_date_syntax)
{
    testSyntax("received", "sa2021",                          "(received >= '2022-01-01T00:00:00+00:00')");
    testSyntax("received", "sa2021-09",                       "(received >= '2021-10-01T00:00:00+00:00')");
    testSyntax("received", "sa2021-09-08",                    "(received >= '2021-09-09T00:00:00+00:00')");
    testSyntax("received", "sa2021-09-08T23:32:58+00:00",     "(received >= '2021-09-08T23:32:59+00:00')");
    testSyntax("received", "sa2021-09-07T23:32:59Z",          "(received >= '2021-09-07T23:33:00+00:00')");
    testSyntax("received", "sa2021-11-11T12:34:22",           "(received >= '2021-11-11T12:34:23+00:00')");
    testSyntax("received", "sa2021-09-07T20:59",              "(received >= '2021-09-07T21:00:00+00:00')");
    testSyntax("received", "sa2021-09-07T20:59Z",             "(received >= '2021-09-07T21:00:00+00:00')");
    testSyntax("received", "sa2021-09-07T20:59+02:00",        "(received >= '2021-09-07T19:00:00+00:00')");
}


/**
 * Due to gt and sa being equivalent when target values are time points, the sa_date_sql test is a copy of the gt_date_sql test.
 */
TEST_F(SearchArgumentTest, sa_date_sql)
{
    // Success with T >= E
    testSql("sent", "sa2011",                          1);
    testSql("sent", "sa2021-08",                       1);
    testSql("sent", "sa2021-09-07",                    1);
    testSql("sent", "sa2021-09-08T12:33:55+00:00",     1);
    testSql("sent", "sa2021-09-08T12:33+00:00",        1);

    // Fail for T < E
    testSql("sent", "sa2021",                          0);
    testSql("sent", "sa2021-09",                       0);
    testSql("sent", "sa2021-09-08",                    0);
    testSql("sent", "sa2021-09-08T12:34:56+00:00",     0);
    testSql("sent", "sa2021-09-08T12:34+00:00",        0);
    testSql("sent", "sa2021-09-08T12:34",              0);
}


/**
 * Due to lt and eb being equivalent when target values are time points, the eb_date_syntax test is a copy of the lt_date_syntax test.
 */
TEST_F(SearchArgumentTest, eb_date_syntax)
{
    testSyntax("received", "eb2021",                          "(received < '2021-01-01T00:00:00+00:00')");
    testSyntax("received", "eb2021-09",                       "(received < '2021-09-01T00:00:00+00:00')");
    testSyntax("received", "eb2021-09-08",                    "(received < '2021-09-08T00:00:00+00:00')");
    testSyntax("received", "eb2021-09-08T23:32:58+00:00",     "(received < '2021-09-08T23:32:58+00:00')");
    testSyntax("received", "eb2021-09-07T23:32:59Z",          "(received < '2021-09-07T23:32:59+00:00')");
    testSyntax("received", "eb2021-11-11T12:34:22",           "(received < '2021-11-11T12:34:22+00:00')");
    testSyntax("received", "eb2021-09-07T20:59",              "(received < '2021-09-07T20:59:00+00:00')");
    testSyntax("received", "eb2021-09-07T20:59Z",             "(received < '2021-09-07T20:59:00+00:00')");
    testSyntax("received", "eb2021-09-07T20:59+02:00",        "(received < '2021-09-07T18:59:00+00:00')");
}


/**
 * Due to lt and eb being equivalent when target values are time points, the eb_date_sql test is a copy of the lt_date_sql test.
 */
TEST_F(SearchArgumentTest, eb_date_sql)
{
    // Success with T < B
    testSql("sent", "eb2022",                          1);
    testSql("sent", "eb2021-10",                       1);
    testSql("sent", "eb2021-09-09",                    1);
    testSql("sent", "eb2021-09-08T23:34:57+00:00",     1);
    testSql("sent", "eb2021-09-08T12:35+00:00",        1);
    testSql("sent", "eb2021-09-08T12:35",              1);

    // Fail for T >= B
    testSql("sent", "eb2021",                          0);
    testSql("sent", "eb2021-09",                       0);
    testSql("sent", "eb2021-09-08",                    0);
    testSql("sent", "eb2021-09-08T12:34:56+00:00",     0);
    testSql("sent", "eb2021-09-08T12:34+00:00",        0);
    testSql("sent", "eb2021-09-08T12:34",              0);
}


TEST_F(SearchArgumentTest, data_failForFractionalSeconds)
{
    for (const std::string prefix : {"", "eq", "ne", "gt", "ge", "lt", "le", "sa", "eb"})
    {
        // Fail for fractional seconds.
        testFailure("sent", prefix + "2021-09-08T12:34:56.123+00:00");

        // Fail even when the fractional seconds are all 0
        testFailure("sent", prefix + "2021-09-08T12:34:56.000+00:00");
    }
}


TEST_F(SearchArgumentTest, eq_syntax)
{
    testSyntax("recipient", "KVNR123456",
               "(recipient = '\\x" + hashedHex("KVNR123456") + "')");
}


TEST_F(SearchArgumentTest, eq_string_sql)
{
    // Success with exact match.
    testSql("recipient", "X-234567890",  1);
    // Success with lowercase character.
    // TODO: Currently not possible to search case-insenitive
    testSql("recipient", "x-234567890",  0);

    // Fail for similar but still different (Y != X) kvnr.
    testSql("recipient", "Y-234567890",  0);

    // Fail for spaces that are not being ignored.
    testSql("recipient", "X-234567890 ", 0);
    testSql("recipient", " X-234567890", 0);
    testSql("recipient", "X-234 567890", 0);
}

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/PostgresBackend.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/database/PostgresConnection.hxx"
#include "shared/database/PostgresConnectionParameters.hxx"
#include "shared/util/Expect.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

#include <exception>
#include <ranges>
#include <thread>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/write.hpp>
#include <gtest/gtest.h>


class PostgresConnectionTest : public testing::Test {
public:
    std::vector<PostgresConnectionParameters> readOnlyParameters()
    {
        using namespace std::string_view_literals;
        std::set preferReadOnlyAttrs = {"read-only"sv, "prefer-standby"sv, "standby"sv};
        if (!PostgresBackend::haveReadOnlyConnection())
        {
            return {};
        }
        auto roParams = PostgresConnection::readOnlyConnectParameters();
        const auto rwParam = PostgresConnection::defaultConnectParameters();
        // ignore any databases that aren't truly read-only'
        std::erase_if(roParams, [&](const PostgresConnectionParameters& p) {
            return (p.host == rwParam.host && p.port == rwParam.port) ||
            ! preferReadOnlyAttrs.contains(p.targetSessionAttrs);
        });
        return roParams;
    }
    static bool usePostgres()
    {
        return TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false);
    }
};

namespace
{
class AutoCloseConnection
{
public:
    AutoCloseConnection()
    {
        boost::asio::ip::tcp::endpoint ep{boost::asio::ip::tcp::v4(), 0};
        mAcceptor.open(ep.protocol());
        mAcceptor.bind(ep);
        mAcceptor.listen();
        co_spawn(mContext, loop(), boost::asio::detached);
        mThread = std::jthread(&boost::asio::io_context::run, std::ref(mContext));
        mStopCallback = std::make_unique<std::stop_callback<std::function<void()>>>(mThread.get_stop_token(), [this] {
            mContext.stop();
        });
    }

    decltype(auto) endpoint()
    {
        return mAcceptor.local_endpoint();
    }

private:
    boost::asio::awaitable<void> loop()
    {
        while (! mThread.get_stop_source().stop_requested())
        {
            auto con = co_await mAcceptor.async_accept(boost::asio::deferred);
            con.close();
        }
    }

    boost::asio::io_context mContext;
    boost::asio::ip::tcp::acceptor mAcceptor{mContext};
    std::unique_ptr<std::stop_callback<std::function<void()>>> mStopCallback;
    std::jthread mThread;
};

class TcpProxy
{
public:
    explicit TcpProxy(std::string hostname, std::string port)
    {
        boost::asio::ip::tcp::resolver resolver{mContext};
        mTargetEndpoints = resolver.resolve(hostname, port);
        Expect(! mTargetEndpoints.empty(), "host not found: " + hostname);

        boost::asio::ip::tcp::endpoint ep{boost::asio::ip::tcp::v4(), 0};
        mAcceptor.open(ep.protocol());
        mAcceptor.bind(ep);
        mAcceptor.listen();
        boost::asio::co_spawn(mContext, loop(), boost::asio::detached);
        mThread = std::jthread(&boost::asio::io_context::run, std::ref(mContext));
        mStopCallback = std::make_unique<std::stop_callback<std::function<void()>>>(mThread.get_stop_token(), [this] {
            mContext.stop();
        });
    }

    boost::asio::ip::tcp::endpoint endpoint()
    {
        return mAcceptor.local_endpoint();
    }

private:
    struct Session : std::enable_shared_from_this<Session> {
        boost::asio::ip::tcp::socket target;
        boost::asio::ip::tcp::socket client;

        Session(boost::asio::ip::tcp::socket c)
            : target(c.get_executor())
            , client(std::move(c))
        {
        }

        boost::asio::awaitable<void> forward(boost::asio::ip::tcp::socket& src, boost::asio::ip::tcp::socket& dst)
        {
            using boost::asio::deferred;
            //NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
            std::array<char, 8192> buffer;
            try {
                for (;;)
                {
                    size_t n = co_await src.async_read_some(boost::asio::buffer(buffer), deferred);
                    co_await async_write(dst, boost::asio::buffer(buffer, n), deferred);
                }
            }
            catch (const boost::system::system_error& err)
            {
                if (err.code() != boost::asio::error::eof)
                {
                    LOG(INFO) << err.what();
                }
            }
        }

        boost::asio::awaitable<void> run(boost::asio::ip::tcp::resolver::results_type target_ep)
        {
            auto executor = co_await boost::asio::this_coro::executor;
            for (const auto& ep: target_ep)
            {
                boost::asio::ip::tcp::socket tgt{executor};
                try {
                    LOG(INFO) << "connecting to: " << ep.endpoint();
                    co_await tgt.async_connect(ep, boost::asio::deferred);
                    target = std::move(tgt);
                    break;
                }
                catch (const std::exception& err)
                {
                    LOG(INFO) << err.what();
                }
            }
            LOG(INFO) << "target connected: " << target.remote_endpoint();

            co_spawn(client.get_executor(), forward(client, target),
                     boost::asio::consign(boost::asio::detached, shared_from_this()));
            co_spawn(client.get_executor(), forward(target, client),
                     boost::asio::consign(boost::asio::detached, shared_from_this()));
        }
    };

    boost::asio::awaitable<void> loop()
    {
        while (! mThread.get_stop_source().stop_requested())
        {
            TLOG(INFO) << "waiting for connection: " << mAcceptor.local_endpoint();
            auto client_socket = co_await mAcceptor.async_accept(boost::asio::deferred);
            TLOG(INFO) << "accepted: " << client_socket.remote_endpoint();
            auto session = std::make_shared<Session>(std::move(client_socket));
            co_spawn(mContext, session->run(mTargetEndpoints), boost::asio::consign(boost::asio::detached, session));
        }
        TLOG(INFO) << "proxy done";

    }

    boost::asio::ip::tcp::resolver::results_type mTargetEndpoints;
    boost::asio::io_context mContext;
    boost::asio::ip::tcp::acceptor mAcceptor{mContext};
    std::unique_ptr<std::stop_callback<std::function<void()>>> mStopCallback;
    std::jthread mThread;
};

}


TEST_F(PostgresConnectionTest, readOnlyConnectParameters)
{
    {
        EnvironmentVariableGuard roHosts{ConfigurationKey::POSTGRES_RO_HOST, ""};
        EXPECT_THROW((void) PostgresConnection::readOnlyConnectParameters(), std::logic_error);
    }
    {
        EnvironmentVariableGuard roHosts{ConfigurationKey::POSTGRES_RO_HOST, "host1"};
        EnvironmentVariableGuard roPorts{ConfigurationKey::POSTGRES_RO_PORT, "3452"};
        std::vector<PostgresConnectionParameters> connParams;
        ASSERT_NO_THROW(connParams = PostgresConnection::readOnlyConnectParameters());
        ASSERT_EQ(connParams.size(), 1);
        const auto& conn0 = connParams[0];
        EXPECT_EQ(conn0.host, "host1");
        EXPECT_EQ(conn0.port, "3452");
    }
    {
        EnvironmentVariableGuard roHosts{ConfigurationKey::POSTGRES_RO_HOST, "host1, host2"};
        EnvironmentVariableGuard roPorts{ConfigurationKey::POSTGRES_RO_PORT, "3452"};
        // intentionally set read-write parameter to test fallback
        EnvironmentVariableGuard dbname{ConfigurationKey::POSTGRES_DATABASE, "test"};
        std::vector<PostgresConnectionParameters> connParams;
        ASSERT_NO_THROW(connParams = PostgresConnection::readOnlyConnectParameters());
        ASSERT_EQ(connParams.size(), 2);
        const auto& conn0 = connParams[0];
        EXPECT_EQ(conn0.host, "host1");
        EXPECT_EQ(conn0.port, "3452");
        EXPECT_EQ(conn0.dbname, "test");
        const auto& conn1 = connParams[1];
        EXPECT_EQ(conn1.host, "host2");
        EXPECT_EQ(conn1.port, "3452");
        EXPECT_EQ(conn1.dbname, "test");
    }
    {
        // different ports; also use different separators
        EnvironmentVariableGuard roHosts{ConfigurationKey::POSTGRES_RO_HOST, "host1; host2"};
        EnvironmentVariableGuard roPorts{ConfigurationKey::POSTGRES_RO_PORT, "3452, 2000"};
        // intentionally set read-write parameter to test fallback
        EnvironmentVariableGuard dbname{ConfigurationKey::POSTGRES_DATABASE, "test"};
        std::vector<PostgresConnectionParameters> connParams;
        ASSERT_NO_THROW(connParams = PostgresConnection::readOnlyConnectParameters());
        ASSERT_EQ(connParams.size(), 2);
        const auto& conn0 = connParams[0];
        EXPECT_EQ(conn0.host, "host1");
        EXPECT_EQ(conn0.port, "3452");
        EXPECT_EQ(conn0.dbname, "test");
        const auto& conn1 = connParams[1];
        EXPECT_EQ(conn1.host, "host2");
        EXPECT_EQ(conn1.port, "2000");
        EXPECT_EQ(conn1.dbname, "test");
    }
}


TEST_F(PostgresConnectionTest, readWrite)
{
    if (! usePostgres())
    {
        GTEST_SKIP() << "database tests disabled";
    }
    auto roParams = readOnlyParameters();
    // let PostgresConnection find out that the target is read-only by overriding targetSessionAttrs
    for (auto& p: roParams)
    {
        p.targetSessionAttrs = "primary";
    }
    auto rwParam = PostgresConnection::defaultConnectParameters();
    AutoCloseConnection autoCloseConnection;
    PostgresConnectionParameters autoCloseParam{rwParam};
    autoCloseParam.host = "localhost";
    autoCloseParam.port = std::to_string(autoCloseConnection.endpoint().port());
    std::vector testDBs{autoCloseParam};
    std::ranges::copy(roParams, std::back_inserter(testDBs));
    testDBs.emplace_back(rwParam);
    PostgresConnection con{testDBs};
    ASSERT_NO_THROW(con.connectIfNeeded());
    auto tx = con.createTransaction(TransactionMode::autocommit);
    auto transactionReadOnly = tx->exec("SHOW transaction_read_only").one_field().as<std::string>();
    EXPECT_EQ(transactionReadOnly, "off");
}

TEST_F(PostgresConnectionTest, preferReadOnly)
{
    if (! TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
    {
        GTEST_SKIP() << "database tests disabled";
    }
    auto roParams = readOnlyParameters();
    if (roParams.empty())
    {
        GTEST_SKIP() << "no read-only database configured";
    }
    const auto rwParam = PostgresConnection::defaultConnectParameters();
    AutoCloseConnection autoCloseConnection;
    PostgresConnectionParameters autoCloseParam{roParams[0]};
    autoCloseParam.port = std::to_string(autoCloseConnection.endpoint().port());
    autoCloseParam.host = "localhost";

    auto test = [&](const std::vector<PostgresConnectionParameters>& testDBs) {
        PostgresConnection con{testDBs};
        ASSERT_NO_THROW(con.connectIfNeeded());
        auto& pqxxCon{static_cast<pqxx::connection&>(con)};
        const auto& connHost = pqxxCon.hostname();
        const auto& connPort = pqxxCon.port();
        bool isReadOnlyConnected = std::ranges::any_of(roParams, [&](const PostgresConnectionParameters& p) {
            return p.host == connHost && p.port == connPort;
        });
        ASSERT_TRUE(isReadOnlyConnected);
    };
    {
        // autoclose, read-write, read-only
        std::vector testDBs{autoCloseParam};
        testDBs.push_back(rwParam);
        std::ranges::copy(roParams, std::back_inserter(testDBs));
        EXPECT_NO_FATAL_FAILURE(test(testDBs));
    }
    {
        // autoclose, read-only, read-write
        std::vector testDBs{autoCloseParam};
        std::ranges::copy(roParams, std::back_inserter(testDBs));
        testDBs.push_back(rwParam);
        EXPECT_NO_FATAL_FAILURE(test(testDBs));
    }
    {
        // read-only, autoclose, read-write
        std::vector testDBs{roParams};
        testDBs.push_back(autoCloseParam);
        testDBs.push_back(rwParam);
        EXPECT_NO_FATAL_FAILURE(test(testDBs));
    }
    {
        // read-write, autoclose, read-only
        std::vector testDBs{rwParam};
        testDBs.push_back(autoCloseParam);
        std::ranges::copy(roParams, std::back_inserter(testDBs));
        EXPECT_NO_FATAL_FAILURE(test(testDBs));
    }
}


TEST_F(PostgresConnectionTest, fallback)
{
    if (! TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
    {
        GTEST_SKIP() << "database tests disabled";
    }
    const auto rwParam = PostgresConnection::defaultConnectParameters();

    AutoCloseConnection autoCloseConnection;
    PostgresConnectionParameters autoCloseParam{rwParam};
    autoCloseParam.port = std::to_string(autoCloseConnection.endpoint().port());
    autoCloseParam.host = "localhost";
    autoCloseParam.targetSessionAttrs = "prefer-standby";
    std::vector testDBs{autoCloseParam};
    testDBs.push_back(rwParam);
    PostgresConnection con{testDBs};
    ASSERT_NO_THROW(con.connectIfNeeded());
}

TEST_F(PostgresConnectionTest, firstFallback)
{
    if (! TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
    {
        GTEST_SKIP() << "database tests disabled";
    }
    const auto rwParam = PostgresConnection::defaultConnectParameters();

    AutoCloseConnection autoCloseConnection;
    PostgresConnectionParameters autoCloseParam{rwParam};
    autoCloseParam.port = std::to_string(autoCloseConnection.endpoint().port());
    autoCloseParam.host = "localhost";
    autoCloseParam.targetSessionAttrs = "prefer-standby";
    std::vector testDBs{autoCloseParam};

    TcpProxy proxyRw{rwParam.host, rwParam.port};
    PostgresConnectionParameters rwProxyParam{rwParam};
    rwProxyParam.port = std::to_string(proxyRw.endpoint().port());
    rwProxyParam.host = "localhost";
    testDBs.push_back(rwProxyParam);

    testDBs.push_back(rwParam);
    PostgresConnection con{testDBs};
    ASSERT_NO_THROW(con.connectIfNeeded());

    auto connInfo = con.getConnectionInfo();
    ASSERT_TRUE(connInfo.has_value());
    EXPECT_EQ(connInfo->hostname, rwProxyParam.host);
    EXPECT_EQ(connInfo->port, rwProxyParam.port);
}


TEST_F(PostgresConnectionTest, fail)
{
    const auto rwParam = PostgresConnection::defaultConnectParameters();
    AutoCloseConnection autoCloseConnection1;
    PostgresConnectionParameters autoCloseParam1{rwParam};
    autoCloseParam1.port = std::to_string(autoCloseConnection1.endpoint().port());
    autoCloseParam1.host = "localhost";
    autoCloseParam1.targetSessionAttrs = "any";
    AutoCloseConnection autoCloseConnection2;
    PostgresConnectionParameters autoCloseParam2{rwParam};
    autoCloseParam2.port = std::to_string(autoCloseConnection1.endpoint().port());
    autoCloseParam2.host = "localhost";
    autoCloseParam2.targetSessionAttrs = "read-only";

    std::vector testDBs{autoCloseParam1, autoCloseParam2};
    PostgresConnection con{testDBs};
    ASSERT_ANY_THROW(con.connectIfNeeded());
}


/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/database/PostgresBackend.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/String.hxx"

#include <gtest/gtest.h>
#include <test/util/EnvironmentVariableGuard.hxx>


class PostgresBackendTest : public testing::Test
{
public:
    void expectContains (const std::string_view& container, const std::string& parameterizedSubject, const std::string& parameter)
    {
        const std::string subject = String::replaceAll(parameterizedSubject, "%%", parameter);
        EXPECT_TRUE(container.find(subject) != std::string::npos)
                        << container << " does not contain " << subject;
    }

    void checkPostgresConnectionString (void)//NOLINT(readability-function-cognitive-complexity)
    {
        const auto connectionString = PostgresBackend::defaultConnectString();
        const auto& configuration = Configuration::instance();

        expectContains(connectionString, "host='%%'", configuration.getStringValue(ConfigurationKey::POSTGRES_HOST));
        expectContains(connectionString, "port='%%'", configuration.getStringValue(ConfigurationKey::POSTGRES_PORT));
        expectContains(connectionString, "dbname='%%'", configuration.getStringValue(ConfigurationKey::POSTGRES_DATABASE));
        expectContains(connectionString, "user='%%'", configuration.getStringValue(ConfigurationKey::POSTGRES_USER));
        expectContains(connectionString, "target_session_attrs=read-write", "");
        expectContains(connectionString, "connect_timeout=%%", configuration.getStringValue(ConfigurationKey::POSTGRES_CONNECT_TIMEOUT_SECONDS));
        expectContains(connectionString, "tcp_user_timeout=%%", configuration.getStringValue(ConfigurationKey::POSTGRES_TCP_USER_TIMEOUT_MS));
        expectContains(connectionString, "keepalives=1", "");
        expectContains(connectionString, "keepalives_idle=%%", configuration.getStringValue(ConfigurationKey::POSTGRES_KEEPALIVES_IDLE_SEC));
        expectContains(connectionString, "keepalives_interval=%%", configuration.getStringValue(ConfigurationKey::POSTGRES_KEEPALIVES_INTERVAL_SEC));
        expectContains(connectionString, "keepalives_count=%%", configuration.getStringValue(ConfigurationKey::POSTGRES_KEEPALIVES_COUNT));


        const std::string password = configuration.getStringValue(ConfigurationKey::POSTGRES_PASSWORD);
        if (password.empty())
            EXPECT_FALSE(connectionString.find("password=") != std::string::npos);
        else
            expectContains(connectionString, "password='%%'", password);

        const auto serverRootCertPath = configuration.getOptionalStringValue(ConfigurationKey::POSTGRES_SSL_ROOT_CERTIFICATE_PATH, "/erp/config/POSTGRES_CERTIFICATE");
        if (serverRootCertPath.empty())
        {
            if (configuration.getOptionalBoolValue(ConfigurationKey::POSTGRES_USESSL, true))
                expectContains(connectionString, "sslmode=require", "");
            else
                EXPECT_FALSE(connectionString.find("sslmode=") != std::string::npos);
        }
        else
        {
            expectContains(connectionString, "sslmode=verify-full", "");
            expectContains(connectionString, "sslrootcert='%%'", serverRootCertPath);

            const auto sslCertificatePath = String::trim(configuration.getOptionalStringValue(ConfigurationKey::POSTGRES_SSL_CERTIFICATE_PATH, ""));
            const auto sslKeyPath = String::trim(configuration.getOptionalStringValue(ConfigurationKey::POSTGRES_SSL_KEY_PATH, ""));

            if ( ! (sslCertificatePath.empty() || sslKeyPath.empty()))
            {
                expectContains(connectionString, "sslcert='%%'", sslCertificatePath);
                expectContains(connectionString, "sslkey='%%'", sslKeyPath);
            }
            else
            {
                EXPECT_FALSE(connectionString.find("sslcert=") != std::string::npos);
                EXPECT_FALSE(connectionString.find("sslkey=") != std::string::npos);
            }
        }

        const bool enableScramAuthentication = configuration.getOptionalBoolValue(ConfigurationKey::POSTGRES_ENABLE_SCRAM_AUTHENTICATION, false);
        if (enableScramAuthentication)
            expectContains(connectionString, "channel_binding=require", "");
        else
            EXPECT_FALSE(connectionString.find("channel_binding=") != std::string::npos);
    }
};


TEST_F(PostgresBackendTest, defaultConnectString)
{
    checkPostgresConnectionString();
}


TEST_F(PostgresBackendTest, connectString_verifyFull_withBoth)
{
    EnvironmentVariableGuard sslrootcert ("ERP_POSTGRES_CERTIFICATE_PATH", "root");
    EnvironmentVariableGuard sslcert ("ERP_POSTGRES_SSL_CERTIFICATE_PATH", "foo");
    EnvironmentVariableGuard sslkey ("ERP_POSTGRES_SSL_KEY_PATH", "bar");

    checkPostgresConnectionString();
}

TEST_F(PostgresBackendTest, connectString_verifyFull_withSslCert)
{
    EnvironmentVariableGuard sslrootcert ("ERP_POSTGRES_CERTIFICATE_PATH", "root");
    EnvironmentVariableGuard sslcert ("ERP_POSTGRES_SSL_CERTIFICATE_PATH", "foo");
    EnvironmentVariableGuard sslkey ("ERP_POSTGRES_SSL_KEY_PATH", "");

    checkPostgresConnectionString();
}

TEST_F(PostgresBackendTest, connectString_verifyFull_withSslKey)
{
    EnvironmentVariableGuard sslrootcert ("ERP_POSTGRES_CERTIFICATE_PATH", "root");
    EnvironmentVariableGuard sslcert ("ERP_POSTGRES_SSL_CERTIFICATE_PATH", "");
    EnvironmentVariableGuard sslkey ("ERP_POSTGRES_SSL_KEY_PATH", "bar");

    checkPostgresConnectionString();
}

TEST_F(PostgresBackendTest, connectString_verifyFull_withoutBoth)
{
    EnvironmentVariableGuard sslrootcert ("ERP_POSTGRES_CERTIFICATE_PATH", "root");
    EnvironmentVariableGuard sslcert ("ERP_POSTGRES_SSL_CERTIFICATE_PATH", "");
    EnvironmentVariableGuard sslkey ("ERP_POSTGRES_SSL_KEY_PATH", "");

    checkPostgresConnectionString();
}

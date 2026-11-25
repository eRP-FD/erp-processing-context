/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/Configuration.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/ErpConstants.hxx"
#include "test_config.h"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>


class ConfigurationTest : public testing::Test
{
public:
    void SetUp (void) override
    {
    }

    std::unique_ptr<Configuration> createConfiguration() const
    {
        return std::make_unique<Configuration>();
    }

    using ScopedSetEnv = EnvironmentVariableGuard;
    Configuration configuration;
};


TEST_F(ConfigurationTest, getStringValue)
{
    ScopedSetEnv scopeEnv(configuration.getEnvironmentVariableName(ConfigurationKey::SERVER_THREAD_COUNT), "17");
    const std::string value = Configuration::instance().getStringValue(ConfigurationKey::SERVER_THREAD_COUNT);

    ASSERT_EQ(value, "17");
}


TEST_F(ConfigurationTest, getSafeStringValue)
{
    ScopedSetEnv scopeEnv(configuration.getEnvironmentVariableName(ConfigurationKey::SERVER_THREAD_COUNT), "17");
    const SafeString value = Configuration::instance().getSafeStringValue(ConfigurationKey::SERVER_THREAD_COUNT);

    ASSERT_EQ(static_cast<std::string_view>(value), "17");
}


TEST_F(ConfigurationTest, getIntValue)
{
    ScopedSetEnv scopeEnv(configuration.getEnvironmentVariableName(ConfigurationKey::SERVER_THREAD_COUNT), "17");
    const auto value = Configuration::instance().getIntValue(ConfigurationKey::SERVER_THREAD_COUNT);

    ASSERT_EQ(value, 17);
}


TEST_F(ConfigurationTest, getOptionalIntValue)
{
    EnvironmentVariableGuard envGuard{ErpConstants::ConfigurationFileNameVariable,
            std::string(TEST_DATA_DIR) + "/configuration-getOptionalIntValue.json"};

    const auto configuration = createConfiguration();

    const auto value = configuration->getOptionalIntValue(ConfigurationKey::SERVER_THREAD_COUNT, 7684);

    ASSERT_EQ(value, 7684);
}


TEST_F(ConfigurationTest, getBoolValue)
{
    ScopedSetEnv scopeEnv(configuration.getEnvironmentVariableName(ConfigurationKey::POSTGRES_ENABLE_SCRAM_AUTHENTICATION), "true");
    bool value = Configuration::instance().getBoolValue(ConfigurationKey::POSTGRES_ENABLE_SCRAM_AUTHENTICATION);

    ASSERT_EQ(value, true);
}

TEST_F(ConfigurationTest, getBoolValueFromJson)
{
    bool value = Configuration::instance().getBoolValue(ConfigurationKey::POSTGRES_USESSL);

    ASSERT_EQ(value, true);
}

TEST_F(ConfigurationTest, getOptionalBoolValue)
{
    bool value = Configuration::instance().getOptionalBoolValue(ConfigurationKey::POSTGRES_ENABLE_SCRAM_AUTHENTICATION, false);

    ASSERT_EQ(value, false);
}


TEST_F(ConfigurationTest, getArrayFromEnvironment)//NOLINT(readability-function-cognitive-complexity)
{
    ScopedSetEnv configFileEnv(ErpConstants::ConfigurationFileNameVariable,
                               ResourceManager::getAbsoluteFilename("test/configuration-emptyObject.json").string());
    const auto configuration = createConfiguration();
    const auto key = ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS;
    const auto& varName = configuration->getEnvironmentVariableName(key);
    {
        ScopedSetEnv scopeEnv(varName, {});
        ASSERT_ANY_THROW(configuration->getArray(key));
    }
    {
        ScopedSetEnv scopeEnv(varName, "value1;value2;value3");
        EXPECT_EQ(configuration->getArray(key), (std::vector<std::string>{"value1", "value2", "value3"}));
    }
    {
        ScopedSetEnv scopeEnv(varName, ";value2;value3");
        EXPECT_EQ(configuration->getArray(key), (std::vector<std::string>{"", "value2", "value3"}));
    }
    {
        ScopedSetEnv scopeEnv(varName, "value1;;value3");
        EXPECT_EQ(configuration->getArray(key), (std::vector<std::string>{"value1", "", "value3"}));
    }
    {
        ScopedSetEnv scopeEnv(varName, "value1;value2;");
        EXPECT_EQ(configuration->getArray(key), (std::vector<std::string>{"value1", "value2", ""}));
    }
    {
        ScopedSetEnv scopeEnv(varName, ";;");
        EXPECT_EQ(configuration->getArray(key), (std::vector<std::string>{"", "", ""}));
    }
}

TEST_F(ConfigurationTest, getArrayFromFile)
{
    ScopedSetEnv configFileEnv(ErpConstants::ConfigurationFileNameVariable,
                               ResourceManager::getAbsoluteFilename("test/configuration-getArray.json").string());
    const auto key = ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS;
    const auto& varName = configuration.getEnvironmentVariableName(key);
    ScopedSetEnv scopeEnv(varName, {});
    EXPECT_EQ(createConfiguration()->getArray(key), (std::vector<std::string>{"value1", "value2"}));
}

TEST_F(ConfigurationTest, OptionalStringValue_null)
{
    static constexpr auto key = ConfigurationKey::PUBLIC_E_PRESCRIPTION_SERVICE_URL;
    {
        // first check that this is configured in some other config file
        ScopedSetEnv configFileEnv(
            ErpConstants::ConfigurationFileNameVariable,
            ResourceManager::getAbsoluteFilename("test/configuration-emptyObject.json").string());
        EXPECT_FALSE(createConfiguration()->getOptionalStringValue(key).has_value());
    }
    {
        // now check, that we can unset it by providing null as value
        ScopedSetEnv configFileEnv(ErpConstants::ConfigurationFileNameVariable,
                                   ResourceManager::getAbsoluteFilename("test/configuration-null.json").string());
        EXPECT_FALSE(createConfiguration()->getOptionalStringValue(key).has_value());
    }
}

TEST_F(ConfigurationTest, OptionalSafeStringValue_null)
{
    static constexpr auto key = ConfigurationKey::PUBLIC_E_PRESCRIPTION_SERVICE_URL;
    {
        // first check that this is configured in some other config file
        ScopedSetEnv configFileEnv(
            ErpConstants::ConfigurationFileNameVariable,
            ResourceManager::getAbsoluteFilename("test/configuration-emptyObject.json").string());
        EXPECT_FALSE(createConfiguration()->getOptionalSafeStringValue(key).has_value());
    }
    {
        // now check, that we can unset it by providing null as value
        ScopedSetEnv configFileEnv(ErpConstants::ConfigurationFileNameVariable,
                                   ResourceManager::getAbsoluteFilename("test/configuration-null.json").string());
        EXPECT_FALSE(createConfiguration()->getOptionalSafeStringValue(key).has_value());
    }
}

TEST_F(ConfigurationTest, pushGatewayFQDNs)
{
    std::set<Configuration::PushGatewayFQDNs> fqdns;
    {
        const ScopedSetEnv PUSH_GATEWAY_FQDN_ALLOW_LIST(ConfigurationKey::PUSH_GATEWAY_FQDN_ALLOW_LIST, "localhost");
        ASSERT_NO_THROW(fqdns = Configuration::instance().pushGatewayFQDNs());
        ASSERT_EQ(fqdns.size(), 1);
        ASSERT_EQ(fqdns.begin()->hostName, "localhost");
        ASSERT_EQ(fqdns.begin()->port, 443);
    }
    {
        const ScopedSetEnv PUSH_GATEWAY_FQDN_ALLOW_LIST(ConfigurationKey::PUSH_GATEWAY_FQDN_ALLOW_LIST,
                                                        "localhost:65535;pushgw.erezept.gematik.de");
        ASSERT_NO_THROW(fqdns = Configuration::instance().pushGatewayFQDNs());
        ASSERT_EQ(fqdns.size(), 2);
        auto it = fqdns.begin();
        ASSERT_EQ(it->hostName, "localhost");
        ASSERT_EQ(it->port, 65535);
        ++it;
        ASSERT_EQ(it->hostName, "pushgw.erezept.gematik.de");
        ASSERT_EQ(it->port, 443);
    }
    {
        const ScopedSetEnv PUSH_GATEWAY_FQDN_ALLOW_LIST(ConfigurationKey::PUSH_GATEWAY_FQDN_ALLOW_LIST,
                                                        "localhost:14443;pushgw.erezept.gematik.de:0");
        EXPECT_THROW(fqdns = Configuration::instance().pushGatewayFQDNs(), std::runtime_error);
    }
    {
        const ScopedSetEnv PUSH_GATEWAY_FQDN_ALLOW_LIST(ConfigurationKey::PUSH_GATEWAY_FQDN_ALLOW_LIST,
                                                        "localhost:65536");
        EXPECT_THROW(fqdns = Configuration::instance().pushGatewayFQDNs(), std::runtime_error);
    }
}

TEST_F(ConfigurationTest, getOptionalPem)
{
    static constexpr auto testKey = ConfigurationKey::MEDICATION_EXPORTER_PUSH_CLIENT_SERVER_CA;
    const auto& config = Configuration::instance();
    auto pemFile = ResourceManager::getAbsoluteFilename("test/generated_pki_push/rootCA/rootCA-cert.pem");
    auto expected = FileHelper::readFileAsString(pemFile);
    ASSERT_FALSE(expected.empty()); // make sure this test isn't trivial
    {
        // use prefix: "pem:"
        EnvironmentVariableGuard pemGuard{testKey,"pem:" + expected};
        std::optional<std::string> actual;
        ASSERT_NO_THROW(actual = config.getOptionalPemValue(testKey));
        EXPECT_EQ(actual, expected);
    }
    {
        // use prefix: "file://"
        EnvironmentVariableGuard pemGuard{testKey,"file://" + pemFile.native()};
        std::optional<std::string> actual;
        ASSERT_NO_THROW(actual = config.getOptionalPemValue(testKey));
        EXPECT_EQ(actual, expected);
    }
    {
        // no prefix file name
        EnvironmentVariableGuard pemGuard{testKey, pemFile.native()};
        std::optional<std::string> actual;
        ASSERT_THROW(actual = config.getOptionalPemValue(testKey), IllegalPemPrefixError);
    }
    {
        // no prefix pem
        EnvironmentVariableGuard pemGuard{testKey, expected};
        std::optional<std::string> actual;
        ASSERT_THROW(actual = config.getOptionalPemValue(testKey), IllegalPemPrefixError);
    }
}

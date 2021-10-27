/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/Configuration.hxx"
#include "erp/util/Environment.hxx"
#include "erp/ErpConstants.hxx"
#include "test_config.h"
#include "tools/ResourceManager.hxx"

#include <gtest/gtest.h>


class ConfigurationTest : public testing::Test
{
public:
    void SetUp (void) override
    {
    }

    std::unique_ptr<Configuration> createConfiguration() const
    {
        return std::unique_ptr<Configuration>(new Configuration);
    }

    class ScopedSetEnv
    {
    public:
        ScopedSetEnv(std::string name, const std::optional<std::string>& value)
            : mName(std::move(name))
            , mOldValue(Environment::get(name))
        {
            setFromOptional(mName, value);
        }

        ~ScopedSetEnv()
        {
            setFromOptional(mName, mOldValue);
        }

    private:
        void setFromOptional(const std::string& name, const std::optional<std::string>& value)
        {
            if (value.has_value())
                Environment::set(name, value.value());
            else
                Environment::unset(name);
        }

        std::string mName;
        std::optional<std::string> mOldValue;
    };

};


TEST_F(ConfigurationTest, getStringValue)
{
    ScopedSetEnv scopeEnv(Configuration::getEnvironmentVariableName(ConfigurationKey::SERVER_THREAD_COUNT), "17");
    const std::string value = Configuration::instance().getStringValue(ConfigurationKey::SERVER_THREAD_COUNT);

    ASSERT_EQ(value, "17");
}


TEST_F(ConfigurationTest, getSafeStringValue)
{
    ScopedSetEnv scopeEnv(Configuration::getEnvironmentVariableName(ConfigurationKey::SERVER_THREAD_COUNT), "17");
    const SafeString value = Configuration::instance().getSafeStringValue(ConfigurationKey::SERVER_THREAD_COUNT);

    ASSERT_EQ(static_cast<std::string_view>(value), "17");
}


TEST_F(ConfigurationTest, getIntValue)
{
    ScopedSetEnv scopeEnv(Configuration::getEnvironmentVariableName(ConfigurationKey::SERVER_THREAD_COUNT), "17");
    const size_t value = Configuration::instance().getIntValue(ConfigurationKey::SERVER_THREAD_COUNT);

    ASSERT_EQ(value, 17u);
}


TEST_F(ConfigurationTest, getOptionalIntValue)
{
    auto oldValue = Environment::get(ErpConstants::ConfigurationFileNameVariable);
    Environment::set(ErpConstants::ConfigurationFileNameVariable,
            std::string(TEST_DATA_DIR) + "/configuration-getOptionalIntValue.json");

    const auto configuration = createConfiguration();

    if (oldValue.has_value())
        Environment::set(ErpConstants::ConfigurationFileNameVariable, oldValue.value());
    else
        Environment::unset(ErpConstants::ConfigurationFileNameVariable);

    const size_t value = configuration->getOptionalIntValue(ConfigurationKey::SERVER_THREAD_COUNT, 7684);

    ASSERT_EQ(value, 7684u);
}


TEST_F(ConfigurationTest, getBoolValue)
{
    ScopedSetEnv scopeEnv(Configuration::getEnvironmentVariableName(ConfigurationKey::POSTGRES_ENABLE_SCRAM_AUTHENTICATION), "true");
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


TEST_F(ConfigurationTest, getArrayFromEnvironment)
{
    ScopedSetEnv configFileEnv(ErpConstants::ConfigurationFileNameVariable,
                               ResourceManager::getAbsoluteFilename("test/configuration-emptyObject.json").string());
    const auto configuration = createConfiguration();
    const auto key = ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS;
    const auto& varName = Configuration::getEnvironmentVariableName(key);
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
    const auto& varName = Configuration::getEnvironmentVariableName(key);
    ScopedSetEnv scopeEnv(varName, {});
    EXPECT_EQ(createConfiguration()->getArray(key), (std::vector<std::string>{"value1", "value2"}));
}

TEST_F(ConfigurationTest, levels)
{
    ScopedSetEnv configFileEnv(ErpConstants::ConfigurationFileNameVariable,
                               ResourceManager::getAbsoluteFilename("test/configuration-levels.json").string());

    const auto key = ConfigurationKey::POSTGRES_HOST;
    const auto& varName = Configuration::getEnvironmentVariableName(key);
    ScopedSetEnv scopeEnv(varName, {});

    {
        const auto* serverHost = "170.0.0.1";
        ScopedSetEnv scopeEnv1(std::string(Configuration::ServerHostEnvVar), serverHost);
        {
            const auto serverPort = 7070;
            ScopedSetEnv scopeEnv2(std::string(Configuration::ServerPortEnvVar), std::to_string(serverPort));
            const auto configuration = createConfiguration();
            EXPECT_EQ(configuration->serverHost(), serverHost);
            EXPECT_EQ(configuration->serverPort(), serverPort);
            EXPECT_EQ(configuration->getStringValue(key), "value1-1-7070");
        }
        {
            const auto serverPort = 7071;
            ScopedSetEnv scopeEnv2(std::string(Configuration::ServerPortEnvVar), std::to_string(serverPort));
            const auto configuration = createConfiguration();
            EXPECT_EQ(configuration->serverHost(), serverHost);
            EXPECT_EQ(configuration->serverPort(), serverPort);
            EXPECT_EQ(configuration->getStringValue(key), "value1-1-7071");
        }
        {
            const auto serverPort = 7072;
            ScopedSetEnv scopeEnv2(std::string(Configuration::ServerPortEnvVar), std::to_string(serverPort));
            const auto configuration = createConfiguration();
            EXPECT_EQ(configuration->serverHost(), serverHost);
            EXPECT_EQ(configuration->serverPort(), serverPort);
            EXPECT_EQ(configuration->getStringValue(key), "value1-1-def");
        }
    }

    {
        const auto* serverHost = "170.0.0.2";
        ScopedSetEnv scopeEnv1(std::string(Configuration::ServerHostEnvVar), serverHost);
        {
            const auto serverPort = 7070;
            ScopedSetEnv scopeEnv2(std::string(Configuration::ServerPortEnvVar), std::to_string(serverPort));
            const auto configuration = createConfiguration();
            EXPECT_EQ(configuration->serverHost(), serverHost);
            EXPECT_EQ(configuration->serverPort(), serverPort);
            EXPECT_EQ(configuration->getStringValue(key), "value1-2-7070");
        }
        {
            const auto serverPort = 7071;
            ScopedSetEnv scopeEnv2(std::string(Configuration::ServerPortEnvVar), std::to_string(serverPort));
            const auto configuration = createConfiguration();
            EXPECT_EQ(configuration->serverHost(), serverHost);
            EXPECT_EQ(configuration->serverPort(), serverPort);
            EXPECT_EQ(configuration->getStringValue(key), "value1-2-def");
        }
    }

    {
        const auto* serverHost = "170.0.0.3";
        const auto serverPort = 7070;
        ScopedSetEnv scopeEnv1(std::string(Configuration::ServerHostEnvVar), serverHost);
        ScopedSetEnv scopeEnv2(std::string(Configuration::ServerPortEnvVar), std::to_string(serverPort));
        const auto configuration = createConfiguration();
        EXPECT_EQ(configuration->serverHost(), serverHost);
        EXPECT_EQ(configuration->serverPort(), serverPort);
        EXPECT_EQ(configuration->getStringValue(key), "value1-common");
    }
}



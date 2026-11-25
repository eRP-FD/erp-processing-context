/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/Base64.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/ConfigurationFormatter.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/ErpConstants.hxx"
#include "test_config.h"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace {
    class TestConfigurationFormatter : public ConfigurationFormatter
    {
        void appendFhirPackagesConfiguration(rapidjson::Document& document [[maybe_unused]]) override{}
        void appendRuntimeConfiguration(rapidjson::Document& document[[maybe_unused]]) override{}
    };
}


class ConfigurationFormatterTest : public testing::Test
{
public:
    rapidjson::Document formatAsJson(ConfigurationKeyFlags flags)
    {
        rapidjson::Document doc;
        [&]{
            const Configuration& config = Configuration::instance();
            TestConfigurationFormatter formatter;
            ASSERT_NO_THROW(doc.Parse(formatter.formatAsJson(config, flags)));
            ASSERT_FALSE(doc.HasParseError());
            ASSERT_TRUE(doc.IsObject());

        }();
        return doc;
    }
};

/* ------------------------------------------------------------------ */
/*  ConfigurationKeyType / KeyData                                    */
/* ------------------------------------------------------------------ */

TEST_F(ConfigurationFormatterTest, KeyDataDefaultTypeIsPlain)
{
    KeyData kd1{"VAR", "/path", ConfigurationKeyFlags::none, "desc"};
    EXPECT_EQ(kd1.type, ConfigurationKeyType::plain);
    EXPECT_TRUE(kd1.hasFlags(ConfigurationKeyFlags::none));
}

TEST_F(ConfigurationFormatterTest, KeyDataExplicitType)
{
    KeyData kd{"VAR", "/path", ConfigurationKeyFlags::credential, ConfigurationKeyType::file, "desc"};
    EXPECT_EQ(kd.type, ConfigurationKeyType::file);
    EXPECT_TRUE(kd.hasFlags(ConfigurationKeyFlags::credential));
    EXPECT_FALSE(kd.hasFlags(ConfigurationKeyFlags::array));
}

TEST_F(ConfigurationFormatterTest, KeyDataHasFlags)
{
    using Flags = ConfigurationKeyFlags;
    KeyData kdPlain{"VAR", "/path", Flags::none, "desc"};
    EXPECT_TRUE(kdPlain.hasFlags(Flags::none));
    EXPECT_FALSE(kdPlain.hasFlags(Flags::credential));

    KeyData kdCred{"VAR", "/path", Flags::credential, ConfigurationKeyType::plain, "desc"};
    EXPECT_TRUE(kdCred.hasFlags(Flags::credential));
    EXPECT_FALSE(kdCred.hasFlags(ConfigurationKeyFlags::array));
}

TEST_F(ConfigurationFormatterTest, ConfigurationKeyFlagsBitwiseOps)
{
    using Flags = ConfigurationKeyFlags;
    auto combined = Flags::credential | Flags::array;
    EXPECT_TRUE((combined & Flags::credential) == Flags::credential);
    EXPECT_TRUE((combined & Flags::array) == Flags::array);
    EXPECT_FALSE((combined & Flags::deprecated) == Flags::deprecated);
}

/* ------------------------------------------------------------------ */
/*  ConfigurationFormatter                                            */
/* ------------------------------------------------------------------ */

TEST_F(ConfigurationFormatterTest, FormatAsJsonFiltersByFlags)
{
    using Flags = ConfigurationKeyFlags;
    rapidjson::Pointer envPtr{"/environment"};
    rapidjson::Pointer funcPtr{"/functional"};

    rapidjson::Document docNone;
    ASSERT_NO_FATAL_FAILURE(docNone = formatAsJson(Flags::none));
    EXPECT_EQ(envPtr.Get(docNone), nullptr);
    EXPECT_EQ(funcPtr.Get(docNone), nullptr);

    rapidjson::Document docMulti;
    ASSERT_NO_FATAL_FAILURE(docMulti = formatAsJson(Flags::categoryEnvironment|Flags::categoryFunctional));
    EXPECT_NE(envPtr.Get(docMulti), nullptr);
    EXPECT_NE(funcPtr.Get(docMulti), nullptr);

    rapidjson::Document docEnv;
    ASSERT_NO_FATAL_FAILURE(docEnv = formatAsJson(Flags::categoryEnvironment));
    EXPECT_NE(envPtr.Get(docEnv), nullptr);
    EXPECT_EQ(funcPtr.Get(docEnv), nullptr);
}

TEST_F(ConfigurationFormatterTest, FileKeyNonExistentPathHandledGracefully)
{
    const EnvironmentVariableGuard guard{"ERP_TSL_INITIAL_CA_DER_PATH", "/nonexistent/path/to/ca.der"};

    rapidjson::Document doc;
    ASSERT_NO_FATAL_FAILURE(doc = formatAsJson(ConfigurationKeyFlags::all));

    // The error field should be set for this key
    rapidjson::Pointer errorPtr{"/environment/ERP_TSL_INITIAL_CA_DER_PATH/error"};
    const auto* const error = errorPtr.Get(doc);
    ASSERT_NE(error, nullptr);
    ASSERT_TRUE(error->IsString());
    const auto& notFoundErr = std::make_error_code(std::errc::no_such_file_or_directory).message();
    EXPECT_STREQ(errorPtr.Get(doc)->GetString(), notFoundErr.c_str());
}

TEST_F(ConfigurationFormatterTest, FileKeyTypeWithValidPemPrefix)
{
    static constexpr char expectedValue[] = "pem:-----BEGIN CERTIFICATE-----\ntest\n-----END CERTIFICATE-----";
    const EnvironmentVariableGuard guard{ConfigurationKey::MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE, expectedValue};

    rapidjson::Document doc;
    ASSERT_NO_FATAL_FAILURE(doc = formatAsJson(ConfigurationKeyFlags::all));

    rapidjson::Pointer valuePtr{"/environment/ERP_MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE/value"};
    const auto* const value = valuePtr.Get(doc);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(value->IsString());
    ASSERT_STREQ(value->GetString(), expectedValue);
}

TEST_F(ConfigurationFormatterTest, FileKeyTypeWithFilePrefix)
{
    static constexpr char testPem[] = "test/tsl/X509Certificate/DefaultOcsp.pem";
    const auto& pemFile = ResourceManager::getAbsoluteFilename(testPem);
    const std::string expectedValue = "file://" + pemFile.native();
    const std::string& expectedContent = ResourceManager::instance().getStringResource(testPem);
    const EnvironmentVariableGuard guard{ConfigurationKey::MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE, expectedValue};

    rapidjson::Document doc;
    ASSERT_NO_FATAL_FAILURE(doc = formatAsJson(ConfigurationKeyFlags::all));

    // The value should be stored (file exists and is valid)
    rapidjson::Pointer valuePtr{"/environment/ERP_MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE/value"};
    const auto* const value = valuePtr.Get(doc);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(value->IsString());
    EXPECT_STREQ(value->GetString(), expectedValue.c_str());

    rapidjson::Pointer errorPtr{"/environment/ERP_MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE/error"};
    const auto* const error = errorPtr.Get(doc);
    ASSERT_EQ(error, nullptr);

    rapidjson::Pointer contentPtr{"/environment/ERP_MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE/content"};
    const auto* const content = contentPtr.Get(doc);
    ASSERT_NE(content, nullptr) <<  [&]{
        rapidjson::StringBuffer buf;
        rapidjson::Writer wr{buf};
        doc.Accept(wr);
        return std::string{buf.GetString(), buf. GetSize()};
    }();
    ASSERT_TRUE(content->IsString());
    EXPECT_STREQ(content->GetString(), expectedContent.c_str());
}


TEST_F(ConfigurationFormatterTest, BinaryKeyTypeWithFilePrefix)
{
    static constexpr char testPem[] = "test/tsl/X509Certificate/nonQesSmcbIssuer.der";
    const auto& pemFile = ResourceManager::getAbsoluteFilename(testPem);
    const std::string expectedValue = "file://" + pemFile.native();
    const std::string& expectedContent = Base64::encode(ResourceManager::instance().getStringResource(testPem));
    const EnvironmentVariableGuard guard{ConfigurationKey::MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE, expectedValue};

    rapidjson::Document doc;
    ASSERT_NO_FATAL_FAILURE(doc = formatAsJson(ConfigurationKeyFlags::all));

    // The value should be stored (file exists and is valid)
    rapidjson::Pointer valuePtr{"/environment/ERP_MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE/value"};
    const auto* const value = valuePtr.Get(doc);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(value->IsString());
    EXPECT_STREQ(value->GetString(), expectedValue.c_str());

    rapidjson::Pointer errorPtr{"/environment/ERP_MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE/error"};
    const auto* const error = errorPtr.Get(doc);
    ASSERT_EQ(error, nullptr);

    rapidjson::Pointer base64Ptr{"/environment/ERP_MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE/base64"};
    const auto* const base64 = base64Ptr.Get(doc);
    ASSERT_NE(base64, nullptr) <<  [&]{
        rapidjson::StringBuffer buf;
        rapidjson::Writer wr{buf};
        doc.Accept(wr);
        return std::string{buf.GetString(), buf. GetSize()};
    }();
    ASSERT_TRUE(base64->IsString());
    EXPECT_STREQ(base64->GetString(), expectedContent.c_str());
}


TEST_F(ConfigurationFormatterTest, FileKeyTypeInvalidPrefix)
{
    const EnvironmentVariableGuard guard{ConfigurationKey::MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE, "just-a-string-no-prefix"};

    rapidjson::Document doc;
    ASSERT_NO_FATAL_FAILURE(doc = formatAsJson(ConfigurationKeyFlags::all));

    // The error field should indicate the invalid prefix
    rapidjson::Pointer errorPtr{"/environment/ERP_MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE/error"};
    const auto* const error = errorPtr.Get(doc);
    ASSERT_NE(error, nullptr);
    ASSERT_TRUE(error->IsString());
    EXPECT_STREQ(error->GetString(), "Value must be prefixed with either pem: or file://");
}

TEST_F(ConfigurationFormatterTest, FileTypeEmptyValue)
{
    const EnvironmentVariableGuard guard{ConfigurationKey::TSL_INITIAL_CA_DER_PATH, ""};

    rapidjson::Document doc;
    ASSERT_NO_FATAL_FAILURE(doc = formatAsJson(ConfigurationKeyFlags::all));

    // The error field should indicate the empty value
    rapidjson::Pointer errorPtr{"/environment/ERP_TSL_INITIAL_CA_DER_PATH/error"};
    const auto* const error = errorPtr.Get(doc);
    ASSERT_NE(error, nullptr);
    ASSERT_TRUE(error->IsString());
    EXPECT_STREQ(error->GetString(), "value is empty");
}

TEST_F(ConfigurationFormatterTest, UnusedVariablesTracked)
{
    static constexpr char ERP_TEST_UNUSED_VAR[] = "ERP_TEST_UNUSED_VAR";
    const EnvironmentVariableGuard guard{ERP_TEST_UNUSED_VAR, "some-value"};

    rapidjson::Document doc;
    ASSERT_NO_FATAL_FAILURE(doc = formatAsJson(ConfigurationKeyFlags::all));

    rapidjson::Pointer unusedPtr{"/unusedVariables"};
    const auto* const unusedVars = unusedPtr.Get(doc);
    ASSERT_NE(unusedVars, nullptr);
    ASSERT_TRUE(unusedVars->IsArray());
    ASSERT_FALSE(unusedVars->Empty());
    const auto& unusedVarsArray = unusedVars->GetArray();
    bool unusedVarFound = std::any_of(unusedVarsArray.Begin(), unusedVarsArray.End(), [](const rapidjson::Value& val) -> bool{
        return val.IsString() && std::string_view{val.GetString(), val.GetStringLength()} == ERP_TEST_UNUSED_VAR;
    });
    EXPECT_TRUE(unusedVarFound);
}

TEST_F(ConfigurationFormatterTest, ArrayConfigFormatted)
{
    rapidjson::Document doc;
    ASSERT_NO_FATAL_FAILURE(doc = formatAsJson(ConfigurationKeyFlags::all));

    rapidjson::Pointer arrayPtr{"/environment/ERP_PUSH_GATEWAY_FQDN_ALLOW_LIST/value"};
    const auto* const array = arrayPtr.Get(doc);
    ASSERT_NE(array, nullptr);
    EXPECT_TRUE(array->IsString());
}

/* ------------------------------------------------------------------ */
/*  Credential redaction                                               */
/* ------------------------------------------------------------------ */

TEST_F(ConfigurationFormatterTest, CredentialRedactedInOutput)
{
    rapidjson::Document doc;
    ASSERT_NO_FATAL_FAILURE(doc = formatAsJson(ConfigurationKeyFlags::all));

    rapidjson::Pointer credPtr{"/environment/ERP_ADMIN_CREDENTIALS/value"};
    const auto* const cred = credPtr.Get(doc);
    ASSERT_NE(cred, nullptr);
    ASSERT_TRUE(cred->IsString());
    EXPECT_STREQ(cred->GetString(), "<redacted>");
}

TEST_F(ConfigurationFormatterTest, PrivateKeyInNonCredential)
{
    static constexpr char testPem[] = "test/qes.pem";
    const auto& pemFile = ResourceManager::getAbsoluteFilename(testPem);
    const std::string expectedValue = "file://" + pemFile.native();
    const EnvironmentVariableGuard guard{ConfigurationKey::MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE, expectedValue};

    rapidjson::Document doc;
    ASSERT_NO_FATAL_FAILURE(doc = formatAsJson(ConfigurationKeyFlags::all));

    // The value should be stored (file exists and is valid)
    rapidjson::Pointer valuePtr{"/environment/ERP_MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE/value"};
    const auto* const value = valuePtr.Get(doc);
    ASSERT_NE(value, nullptr);
    ASSERT_TRUE(value->IsString());
    EXPECT_STREQ(value->GetString(), expectedValue.c_str());

    rapidjson::Pointer errorPtr{"/environment/ERP_MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE/error"};
    const auto* const error = errorPtr.Get(doc);
    ASSERT_NE(error, nullptr);
    ASSERT_TRUE(error->IsString());
    ASSERT_STREQ(error->GetString(), "content contains PRIVATE KEY marker.");

    rapidjson::Pointer contentPtr{"/environment/ERP_MEDICATION_EXPORTER_PUSH_CLIENT_CERTIFICATE/content"};
    const auto* const content = contentPtr.Get(doc);
    ASSERT_EQ(content, nullptr);
}


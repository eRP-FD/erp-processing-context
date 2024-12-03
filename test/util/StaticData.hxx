/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_STATICDATA_HXX
#define ERP_PROCESSING_CONTEXT_STATICDATA_HXX

#include "EnvironmentVariableGuard.hxx"
#include "shared/crypto/Certificate.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "shared/validation/JsonValidator.hxx"
#include "shared/validation/XmlValidator.hxx"
#include "test_config.h"
#include "test/util/TestConfiguration.hxx"

#include <memory>
#include <mutex>

// The sole intention of this classes is to provide one static instance of the JsonValidator class
// in all tests, which takes very long to load, due to the loading of JSON schema files.

class JsonValidatorStatic {
public:
    JsonValidatorStatic()
    {
        auto schemas = Configuration::instance().getArray(ConfigurationKey::JSON_SCHEMA);
        mJsonValidator.loadSchema(
            schemas,
            Configuration::instance().getPathValue(ConfigurationKey::JSON_META_SCHEMA));
    }
    JsonValidator mJsonValidator;
};

class XmlValidatorStatic {
public:
    XmlValidatorStatic()
    {
        configureXmlValidator(mXmlValidator);
        const auto& additional = TestConfiguration::instance().getArray(TestConfigurationKey::TEST_ADDITIONAL_XML_SCHEMAS);
        mXmlValidator.loadSchemas(additional, std::nullopt, std::nullopt);
    }
    XmlValidator mXmlValidator;
};


class StaticData
{
public:
    static std::shared_ptr<JsonValidator> getJsonValidator()
    {
        static auto jsonValidatorStatic = std::make_shared<JsonValidatorStatic>();
        return {jsonValidatorStatic, &jsonValidatorStatic->mJsonValidator};
    }
    static std::shared_ptr<XmlValidator> getXmlValidator()
    {
        static auto xmlValidatorStatic = std::make_shared<XmlValidatorStatic>();
        return {xmlValidatorStatic, &xmlValidatorStatic->mXmlValidator};
    }

    static const Certificate idpCertificate;

    static Factories makeMockFactories();
    static Factories makeMockFactoriesWithServers();
    static PcServiceContext
    makePcServiceContext(std::optional<decltype(Factories::databaseFactory)> cutomDatabaseFactory = {});
};

#endif

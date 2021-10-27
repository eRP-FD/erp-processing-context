/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_STATICDATA_HXX
#define ERP_PROCESSING_CONTEXT_STATICDATA_HXX

#include "erp/crypto/Certificate.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "test/util/TestConfiguration.hxx"

#include <memory>

// The sole intention of this classes is to provide one static instance of the JsonValidator class
// in all tests, which takes very long to load, due to the loading of JSON schema files.

class JsonValidatorStatic {
public:
    JsonValidatorStatic()
    {
        auto schemas = Configuration::instance().getArray(ConfigurationKey::JSON_SCHEMA);
        auto additionalSchemas = TestConfiguration::instance().getArray(TestConfigurationKey::TEST_ADDITIONAL_JSON_SCHEMAS);
        schemas.insert(schemas.end(), additionalSchemas.begin(), additionalSchemas.end());
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
        mXmlValidator.loadSchemas(Configuration::instance().getArray(ConfigurationKey::XML_SCHEMA));
        mXmlValidator.loadSchemas(TestConfiguration::instance().getArray(TestConfigurationKey::TEST_ADDITIONAL_XML_SCHEMAS));
    }
    XmlValidator mXmlValidator;
};


class StaticData
{
public:
    static const std::shared_ptr<JsonValidator> getJsonValidator()
    {
        static auto jsonValidatorStatic = std::make_shared<JsonValidatorStatic>();
        return std::shared_ptr<JsonValidator>(jsonValidatorStatic, &jsonValidatorStatic->mJsonValidator);
    }
    static std::shared_ptr<XmlValidator> getXmlValidator()
    {
        static auto xmlValidatorStatic = std::make_shared<XmlValidatorStatic>();
        return std::shared_ptr<XmlValidator>(xmlValidatorStatic, &xmlValidatorStatic->mXmlValidator);
    }

    static const Certificate idpCertificate;
};

#endif

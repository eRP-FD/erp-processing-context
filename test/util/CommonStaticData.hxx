/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_COMMONSTATICDATA_HXX
#define ERP_PROCESSING_CONTEXT_COMMONSTATICDATA_HXX

#include "shared/util/Configuration.hxx"
#include "shared/validation/JsonValidator.hxx"
#include "shared/validation/XmlValidator.hxx"
#include "test/util/TestConfiguration.hxx"

struct BaseFactories;
class SafeString;
class BaseHttpsServer;
class RequestHandlerManager;
class BaseServiceContext;

// The sole intention of this classes is to provide one static instance of the JsonValidator class
// in all tests, which takes very long to load, due to the loading of JSON schema files.
class JsonValidatorStatic
{
public:
    JsonValidatorStatic()
    {
        auto schemas = Configuration::instance().getArray(ConfigurationKey::JSON_SCHEMA);
        mJsonValidator.loadSchema(schemas, Configuration::instance().getPathValue(ConfigurationKey::JSON_META_SCHEMA));
    }
    JsonValidator mJsonValidator;
};

class XmlValidatorStatic
{
public:
    XmlValidatorStatic()
    {
        configureXmlValidator(mXmlValidator);
        const auto& additional =
            TestConfiguration::instance().getArray(TestConfigurationKey::TEST_ADDITIONAL_XML_SCHEMAS);
        mXmlValidator.loadSchemas(additional, std::nullopt, std::nullopt);
    }
    XmlValidator mXmlValidator;
};


class CommonStaticData
{
public:
    static std::unique_ptr<BaseHttpsServer> nullHttpsServer(const std::string_view address, uint16_t port,
                                                            RequestHandlerManager&& requestHandlers,
                                                            BaseServiceContext& serviceContext,
                                                            bool enforceClientAuthentication,
                                                            const SafeString& caCertificates);


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

protected:
    static void fillMockBaseFactories(BaseFactories& baseFactories);
};

#endif

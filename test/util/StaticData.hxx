/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_STATICDATA_HXX
#define ERP_PROCESSING_CONTEXT_STATICDATA_HXX

#include "erp/crypto/Certificate.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/validation/InCodeValidator.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "test/util/TestConfiguration.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "EnvironmentVariableGuard.hxx"
#include "test_config.h"

#include <memory>

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
        bool oldValid = model::ResourceVersion::supportedBundles()
            .contains(model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01);
        auto validator = oldValid?getValidator<XmlValidatorStatic, true>():getValidator<XmlValidatorStatic, false>();
        return {validator, std::addressof(validator->mXmlValidator)};
    }
    static std::shared_ptr<InCodeValidator> getInCodeValidator()
    {
        static auto inCodeValidatorStatic = std::make_shared<InCodeValidator>();
        return inCodeValidatorStatic;
    }

    static const Certificate idpCertificate;

    static Factories makeMockFactories();
    static Factories makeMockFactoriesWithServers();
    static PcServiceContext makePcServiceContext(std::optional<decltype(Factories::databaseFactory)> cutomDatabaseFactory = {});
private:
    template <typename ValidatorT, bool oldValid>
    static std::shared_ptr<ValidatorT> getValidator()
    {
        bool newValid = model::ResourceVersion::supportedBundles()
            .contains(model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01);
        return newValid?getValidator<ValidatorT, oldValid, true>():getValidator<ValidatorT, oldValid,false>();
    }

    template <typename ValidatorT, bool oldValid, bool newValid>
    static std::shared_ptr<ValidatorT> getValidator()
    {
        static auto validatorStatic = std::make_shared<ValidatorT>();
        return validatorStatic;
    }

};

#endif

#include "test/util/TestConfiguration.hxx"

#include "erp/util/Expect.hxx"


KeyNames TestConfigurationKeyNames::strings (const TestConfigurationKey key)
{
    switch(key)
    {
        case TestConfigurationKey::TEST_USE_DATABASE_MOCK:
            return {"TEST_USE_DATABASE_MOCK", "/test/use-database-mock"};
        case TestConfigurationKey::TEST_ADDITIONAL_JSON_SCHEMAS:
            return {"TEST_ADDITIONAL_JSON_SCHEMAS", "/test/additional-json-schema"};
        case TestConfigurationKey::TEST_ADDITIONAL_XML_SCHEMAS:
            return {"TEST_ADDITIONAL_XML_SCHEMAS", "/test/additional-xml-schema"};
        case TestConfigurationKey::TEST_GEM_RCA3:
            return {"TEST_GEM_RCA3", "/test/GEM.RCA3"};
        case TestConfigurationKey::TEST_GEM_KOMP_CA10:
            return {"TEST_GEM_KOMP_CA10", "/test/GEM.KOMP-CA10"};
        case TestConfigurationKey::TEST_IDP_PUBLIC_KEY:
            return {"TEST_IDP_PUBLIC_KEY", "/test/idp/public-key"};
        case TestConfigurationKey::TEST_IDP_PRIVATE_KEY:
            return {"TEST_IDP_PRIVATE_KEY", "/test/idp/private-key"};
    }
    Fail("unhandled test configuration key");
}


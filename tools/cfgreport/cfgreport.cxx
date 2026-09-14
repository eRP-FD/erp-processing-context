/**
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 **/

#include "shared/util/Configuration.hxx"
#include "shared/util/ConfigurationFormatter.hxx"
#include "fhirtools/repository/views/FhirResourceViewConfiguration.hxx"

#include <cstdlib>
#include <iostream>

class StaticConfigurationFormatter : public ConfigurationFormatter
{
    void appendFhirPackagesConfiguration(rapidjson::Document& document) override {
        ConfigurationFormatter::appendFhirPackagesConfiguration(
            document, Configuration::instance().fhirResourceViewConfiguration<Configuration::ERP>());
    }
    void appendRuntimeConfiguration(rapidjson::Document& document [[maybe_unused]]) override {}
};

int main(int,char*[])
{
    if (!Environment::get("ERP_SERVER_HOST"))
    {
        Environment::set("ERP_SERVER_HOST", "localhost");
    }
    StaticConfigurationFormatter formatter;
    std::cout << formatter.formatAsJson(Configuration::instance(), ConfigurationKeyFlags::all);
    return EXIT_SUCCESS;
}



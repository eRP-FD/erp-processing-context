// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/Application.hxx"
#include "erp/util/ConfigurationFormatter.hxx"
#include "erp/util/RuntimeConfiguration.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/ConfigurationFormatter.hxx"

#include <iostream>

namespace erp
{
void Application::printConfiguration()
{
    using Flags = KeyData::ConfigurationKeyFlags;
    const auto& config = Configuration::instance();
    Fhir::init<ConfigurationBase::ERP>(Fhir::Init::now);
    erp::ConfigurationFormatter formatter(std::make_shared<const erp::RuntimeConfiguration>());
    std::cout << formatter.formatAsJson(config, Flags::all);
}
}

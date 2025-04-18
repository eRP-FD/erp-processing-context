// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "exporter/Application.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/ConfigurationFormatter.hxx"
#include "util/ConfigurationFormatter.hxx"
#include "util/RuntimeConfiguration.hxx"

#include <iostream>

namespace exporter
{
void Application::printConfiguration()
{
    using Flags = KeyData::ConfigurationKeyFlags;
    const auto& config = Configuration::instance();
    Fhir::init<ConfigurationBase::MedicationExporter>(Fhir::Init::now);
    ConfigurationFormatter formatter(std::make_shared<RuntimeConfiguration>());
    std::cout << formatter.formatAsJson(config, Flags::all);
}
}

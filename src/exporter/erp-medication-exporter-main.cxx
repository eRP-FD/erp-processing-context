/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/MedicationExporterMain.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "shared/Application.hxx"


int main(const int argc, const char* argv[], char** /*environment*/)
{
    return Application::MainFn(argc, argv, "erp-medication-exporter", [] () -> int {
        MainStateCondition state(MainState::Unknown);// Only used for tests.
        return MedicationExporterMain::runApplication(MedicationExporterMain::createProductionFactories(), state);
    });
}

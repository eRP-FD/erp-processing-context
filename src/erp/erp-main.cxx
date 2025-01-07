/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/ErpMain.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/Application.hxx"

int main(const int argc, const char* argv[], char** /*environment*/)
{
    return erp::Application{}.MainFn(argc, argv, "erp-processing-context", [] () -> int {
        MainStateCondition state(MainState::Unknown);// Only used for tests.
        return ErpMain::runApplication(ErpMain::createProductionFactories(), state);
    });
}

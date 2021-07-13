#include "erp/erp-serverinfo.hxx"
#include "erp/ErpMain.hxx"
#include "erp/ErpProcessingContext.hxx"
#include "erp/hsm/production/HsmProductionFactory.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/pc/SeedTimer.hxx"
#include "erp/util/SignalHandler.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/TerminationHandler.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "erp/validation/XmlValidator.hxx"

#include <cstdlib>
#include <iostream>
#include <boost/exception/diagnostic_information.hpp>


namespace
{
    constexpr uint16_t DefaultEnrolmentServerPort = 9191;

    void showEnvironment (char** environment)
    {
    #ifdef DEBUG
        size_t index = 0;
        for (char** env = environment; *env!=nullptr; ++env,++index)
        {
            LOG(ERROR) << index << " : " << *env;
        }
    #else
        (void)environment;
    #endif
    }


    void deactivateLibxmlLoggingToStderr()
    {
        // Silence libxml2 by installing a no-op error handler. Without this, errors are logged
        // to stderr, and that would bypass our GLog settings in production.
        xmlSetStructuredErrorFunc(nullptr, [](void*, xmlErrorPtr) {});
    }
}


int main (const int, const char* argv[], char** environment)
{
    int exitCode = EXIT_FAILURE;

    LOG(WARNING) << "Starting erp-processing-context " << ErpServerInfo::ReleaseVersion
                 << " (build: " << ErpServerInfo::BuildVersion << "; " << ErpServerInfo::ReleaseDate << ")";

    try
    {
        ThreadNames::instance().setThreadName(std::this_thread::get_id(), "main");

        GLogConfiguration::init_logging(argv[0]);
        TVLOG(1) << "initialized logging";

        deactivateLibxmlLoggingToStderr();

        showEnvironment(environment);

        ErpMain::StateCondition state (ErpMain::State::Unknown); // Only used for tests.
        exitCode = ErpMain::runApplication(
            DefaultEnrolmentServerPort,
            ErpMain::createProductionFactories(),
            state);
    }
    catch(...)
    {
        LOG(ERROR) << "Unexpected exception: " << boost::current_exception_diagnostic_information();

        // Note that at this point we may or may not have reached the line that waits for the TerminationHandler.
        // In either case, tell the termination handler to terminate the application.
        TerminationHandler::instance().notifyTermination(true);
    }

    VLOG(1) << "notifying TerminationHandler";

    TerminationHandler::instance().notifyTermination(exitCode != EXIT_SUCCESS);

    LOG(WARNING) << "exiting erp-processing-context with exit code " << exitCode;

    return exitCode;
}




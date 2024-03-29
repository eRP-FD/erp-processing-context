/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/ErpMain.hxx"
#include "erp/ErpProcessingContext.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/hsm/production/HsmProductionFactory.hxx"
#include "erp/pc/SeedTimer.hxx"
#include "erp/util/Condition.hxx"
#include "erp/util/ConfigurationFormatter.hxx"
#include "erp/util/CrashHandler.hxx"
#include "erp/util/SignalHandler.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/TerminationHandler.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "erp/util/RuntimeConfiguration.hxx"

#include <boost/exception/diagnostic_information.hpp>
#include <cstdlib>
#include <iostream>


namespace
{
void deactivateLibxmlLoggingToStderr()
{
    // Silence libxml2 by installing a no-op error handler. Without this, errors are logged
    // to stderr, and that would bypass our GLog settings in production.
    xmlSetStructuredErrorFunc(nullptr, [](void*, xmlErrorPtr) {
    });
}

void processCommandLine(const int argc, const char* argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg{argv[i]};
        if (arg == "--print-configuration")
        {
            using Flags = KeyData::ConfigurationKeyFlags;
            const auto& config = Configuration::instance();
            const RuntimeConfiguration runtimeConfiguration;
            std::cout << ConfigurationFormatter::formatAsJson(config, RuntimeConfigurationGetter(runtimeConfiguration),
                                                              Flags::all);
            exit(EXIT_SUCCESS);//NOLINT(concurrency-mt-unsafe)
        }
    }
}
}


int main(const int argc, const char* argv[], char** /*environment*/)
{
    int exitCode = EXIT_FAILURE;

    try
    {
        processCommandLine(argc, argv);
        GLogConfiguration::init_logging(argv[0]);
        ThreadNames::instance().setThreadName(std::this_thread::get_id(), "main");
        CrashHandler::registerSignalHandlers({SIGILL, SIGABRT, SIGSEGV, SIGSYS, SIGFPE, SIGPIPE});

        TLOG(INFO) << "Starting erp-processing-context " << ErpServerInfo::ReleaseVersion()
                   << " (build: " << ErpServerInfo::BuildVersion() << "; " << ErpServerInfo::ReleaseDate() << ")";

        deactivateLibxmlLoggingToStderr();

        ErpMain::StateCondition state(ErpMain::State::Unknown);// Only used for tests.
        exitCode = ErpMain::runApplication(ErpMain::createProductionFactories(), state);
    }
    catch (...)
    {
        TLOG(ERROR) << "Unexpected exception: " << boost::current_exception_diagnostic_information();
    }

    TLOG(INFO) << "exiting erp-processing-context with exit code " << exitCode;

    return exitCode;
}

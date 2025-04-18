/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/Application.hxx"
#include "shared/deprecated/SignalHandler.hxx"
#include "shared/deprecated/TerminationHandler.hxx"
#include "shared/erp-serverinfo.hxx"
#include "shared/hsm/production/HsmProductionFactory.hxx"
#include "shared/util/CrashHandler.hxx"
#include "shared/util/TLog.hxx"
#include "shared/validation/JsonValidator.hxx"
#include "shared/validation/XmlValidator.hxx"

#include <boost/exception/diagnostic_information.hpp>
#include <cstdlib>
#include <iostream>
#include <span>


namespace
{
void deactivateLibxmlLoggingToStderr()
{
    // Silence libxml2 by installing a no-op error handler. Without this, errors are logged
    // to stderr, and that would bypass our GLog settings in production.
    xmlSetStructuredErrorFunc(nullptr, [](void*, const xmlError*) {
    });
}
}

void Application::processCommandLine(const int argc, const char* argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg{argv[i]};
        if (arg == "--print-configuration")
        {
            printConfiguration();
            exit(EXIT_SUCCESS);//NOLINT(concurrency-mt-unsafe)
        }
    }
}


int Application::MainFn(const int argc, const char* argv[], const std::string& name, const std::function<int()>& entryPoint)
{
    int exitCode = EXIT_FAILURE;

    try
        {
            auto args = std::span(argv, size_t(argc));
            processCommandLine(argc, argv);
            GLogConfiguration::initLogging(args[0]);
            ThreadNames::instance().setThreadName(std::this_thread::get_id(), "main");
            CrashHandler::registerSignalHandlers({SIGILL, SIGABRT, SIGSEGV, SIGSYS, SIGFPE});

            TLOG(INFO) << "Starting " << name << " " << ErpServerInfo::ReleaseVersion()
                       << " (build: " << ErpServerInfo::BuildVersion() << "; " << ErpServerInfo::ReleaseDate() << ")";

            deactivateLibxmlLoggingToStderr();

            exitCode = entryPoint();
        }
    catch (...)
        {
            TLOG(ERROR) << "Unexpected exception: " << boost::current_exception_diagnostic_information();
        }

    TLOG(INFO) << "exiting " << name << " with exit code " << exitCode;

    return exitCode;
}

/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

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
    void deactivateLibxmlLoggingToStderr()
    {
        // Silence libxml2 by installing a no-op error handler. Without this, errors are logged
        // to stderr, and that would bypass our GLog settings in production.
        xmlSetStructuredErrorFunc(nullptr, [](void*, xmlErrorPtr) {});
    }

    std::map<int, sighandler_t> old_handler;

    void erp_signal_handler(int sig)
    {
        std::string message = "\nreceived signal " + std::to_string(sig) + "\n";
        const auto res = write(STDERR_FILENO, message.data(), message.size());
        (void) res;
        if (old_handler.count(sig) > 0)
        {
            (void)signal(sig, old_handler[sig]);
        }
        else
        {
            (void)signal(sig, SIG_DFL);
        }
        (void)raise(sig);
    }

    void register_signals(std::initializer_list<int> signals)
    {
        for (const auto& sig : signals)
        {
            old_handler[sig] = signal(sig, &erp_signal_handler);
        }
    }
}


int main (const int, const char* argv[], char** /*environment*/)
{
    register_signals({SIGILL, SIGABRT, SIGSEGV, SIGFPE, SIGINT, SIGTERM
#ifdef SIGSYS
                      ,SIGSYS
#endif
    }
    );

    int exitCode = EXIT_FAILURE;


    try
    {
        GLogConfiguration::init_logging(argv[0]);

        LOG(WARNING) << "Starting erp-processing-context " << ErpServerInfo::ReleaseVersion
                    << " (build: " << ErpServerInfo::BuildVersion << "; " << ErpServerInfo::ReleaseDate << ")";

        ThreadNames::instance().setThreadName(std::this_thread::get_id(), "main");


        deactivateLibxmlLoggingToStderr();

        ErpMain::StateCondition state (ErpMain::State::Unknown); // Only used for tests.
        exitCode = ErpMain::runApplication(
            ErpMain::createProductionFactories(),
            state);
    }
    catch(...)
    {
        LOG(ERROR) << "Unexpected exception: " << boost::current_exception_diagnostic_information();
    }

    LOG(WARNING) << "exiting erp-processing-context with exit code " << exitCode;

    return exitCode;
}




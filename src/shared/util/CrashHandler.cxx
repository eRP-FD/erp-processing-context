/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "shared/util/CrashHandler.hxx"

#include <execinfo.h>
#include <unistd.h>
#include <csignal>
#include <cstdlib>
#include <string>


static constexpr int SIGNAL_STACK_SIZE = 30;

namespace
{
[[noreturn]] void signalHandler(int signalNum)
{
    void* stackInfo[SIGNAL_STACK_SIZE];
    const int size = backtrace(stackInfo, SIGNAL_STACK_SIZE);

    std::string message = "\nError: signal " + std::to_string(signalNum) + ", call stack:\n";
    const auto res = write(STDERR_FILENO, message.data(), message.size());
    (void) res;

    backtrace_symbols_fd(stackInfo, size, STDERR_FILENO);

    exit(1); //NOLINT(concurrency-mt-unsafe)
}
}// namespace


void CrashHandler::registerSignalHandlers(const std::vector<int>& signals)
{
    for (int signalNum : signals)
    {
        if (signal(signalNum, signalHandler) == SIG_ERR)
        {
            // use perror(), since it is thread-safe and easier to use than strerror_r().
            perror("Failed to install signal handler");
        }
    }
}

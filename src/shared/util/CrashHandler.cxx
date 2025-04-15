/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "shared/util/CrashHandler.hxx"

#include <execinfo.h>
#include <unistd.h>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <string>


static constexpr int SIGNAL_STACK_SIZE = 30;

namespace
{
[[noreturn]] void signalHandler(int signalNum)
{
    void* stackInfo[SIGNAL_STACK_SIZE];
    const int size = backtrace(stackInfo, SIGNAL_STACK_SIZE);

    const auto* str1 = "\nError: signal ";
    const auto* str2 = sigabbrev_np(signalNum);
    const auto* str3 = ", call stack:\n";
    auto res = write(STDERR_FILENO, str1, strlen(str1));
    res = write(STDERR_FILENO, str2, strlen(str2));
    res = write(STDERR_FILENO, str3, strlen(str3));
    (void) res;

    backtrace_symbols_fd(stackInfo, size, STDERR_FILENO);

    _exit(1);
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

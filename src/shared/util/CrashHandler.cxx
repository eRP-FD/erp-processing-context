/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "shared/util/CrashHandler.hxx"

#include <execinfo.h>
#include <unistd.h>
#include <array>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>


static constexpr int SIGNAL_STACK_SIZE = 30;
// Must be large enough for backtrace() and backtrace_symbols_fd().
static constexpr std::size_t ALTERNATE_STACK_BYTES = 64UL * 1024UL;
// Upper limit for the time the first crashing thread may need to write its report.
static constexpr unsigned int CRASH_REPORT_TIMEOUT_SECONDS = 5;

namespace
{
std::atomic_flag handlingCrash = ATOMIC_FLAG_INIT; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void writeStr(const char* str)
{
    auto res = write(STDERR_FILENO, str, strlen(str));
    (void) res;
}

void writeHex(std::uintptr_t value)
{
    static constexpr std::array<char, 16> hexDigits{'0','1','2','3','4','5','6','7',
                                                     '8','9','a','b','c','d','e','f'};
    std::array<char, 2 + (sizeof(value) * 2)> buffer{};
    std::size_t pos = buffer.size();
    do // NOLINT(cppcoreguidelines-avoid-do-while)
    {
        buffer.at(--pos) = hexDigits.at(value & 0xfU);
        value >>= 4U;
    } while (value != 0U);
    buffer.at(--pos) = 'x';
    buffer.at(--pos) = '0';
    auto res = write(STDERR_FILENO, buffer.data() + pos, buffer.size() - pos);
    (void) res;
}

[[noreturn]] void signalHandler(int signalNum, siginfo_t* info, void* /*context*/)
{
    // Only the first crashing thread reports. Others wait until it has finished and terminated the process.
    // This avoids interleaved output and concurrent use of backtrace() from several threads.
    if (handlingCrash.test_and_set())
    {
        for (;;)
        {
            pause();
        }
    }
    // The handler must never hang the process: SIGALRM has the default action "terminate".
    alarm(CRASH_REPORT_TIMEOUT_SECONDS);

    std::array<void*, SIGNAL_STACK_SIZE> stackInfo{};
    const int size = backtrace(stackInfo.data(), SIGNAL_STACK_SIZE);

    writeStr("\nError: signal ");
    writeStr(sigabbrev_np(signalNum));
    if (info != nullptr && (signalNum == SIGSEGV || signalNum == SIGILL || signalNum == SIGFPE))
    {
        writeStr(" at address ");
        writeHex(reinterpret_cast<std::uintptr_t>(info->si_addr)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
    writeStr(", call stack:\n");

    backtrace_symbols_fd(stackInfo.data(), size, STDERR_FILENO);

    _exit(1);
}

/// Owns the alternate signal stack of one thread.
class AlternateStack
{
public:
    AlternateStack()
        : mMemory{std::make_unique<char[]>(ALTERNATE_STACK_BYTES)} // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
    {
        stack_t stack{};
        stack.ss_sp = mMemory.get();
        stack.ss_size = ALTERNATE_STACK_BYTES;
        stack.ss_flags = 0;
        if (sigaltstack(&stack, nullptr) != 0)
        {
            perror("Failed to install alternate signal stack");
        }
    }

    ~AlternateStack()
    {
        stack_t disable{};
        disable.ss_flags = SS_DISABLE;
        sigaltstack(&disable, nullptr);
    }

    AlternateStack(const AlternateStack&) = delete;
    AlternateStack& operator=(const AlternateStack&) = delete;
    AlternateStack(AlternateStack&&) = delete;
    AlternateStack& operator=(AlternateStack&&) = delete;

private:
    std::unique_ptr<char[]> mMemory; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
};
}// namespace


void CrashHandler::installAlternateStack()
{
    thread_local const AlternateStack alternateStack;
    (void) alternateStack;
}


void CrashHandler::registerSignalHandlers(const std::vector<int>& signals)
{
    // The first call to backtrace() may load libgcc dynamically (malloc, dlopen), which must not happen
    // inside the signal handler.
    std::array<void*, 1> preload{};
    (void) backtrace(preload.data(), 1);

    installAlternateStack();

    for (const int signalNum : signals)
    {
        struct sigaction action{};
        action.sa_sigaction = signalHandler;
        sigemptyset(&action.sa_mask);
        // SA_ONSTACK:   run on the alternate stack, to survive stack overflow and corrupted stacks.
        // SA_RESETHAND: if the handler itself crashes, the default action terminates the process.
        action.sa_flags = static_cast<int>(SA_SIGINFO | SA_ONSTACK | SA_RESETHAND);
        if (sigaction(signalNum, &action, nullptr) != 0)
        {
            // use perror(), since it is thread-safe and easier to use than strerror_r().
            perror("Failed to install signal handler");
        }
    }
}

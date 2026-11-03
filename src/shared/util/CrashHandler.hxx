/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_CRASHHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_CRASHHANDLER_HXX

#include <vector>

/**
 * This class installs a simple signal handler, and when one
 * of these signals is caught, it will print the stack trace
 * and exit the program. Thus, it is mostly useful for signals
 * like SIGSEGV, SIGABRT, etc.
 *
 * The handler runs on an alternate signal stack, so that it can still produce output when the
 * crash was caused by stack corruption or stack exhaustion. Since an alternate signal stack is a
 * per-thread setting, every thread that should be covered has to call installAlternateStack().
 * registerSignalHandlers() does this for the calling thread.
 */
class CrashHandler
{
public:
    static void registerSignalHandlers(const std::vector<int>& signals);

    /**
     * Allocates and installs an alternate signal stack for the calling thread.
     * Idempotent per thread; the stack is released when the thread exits.
     * Must be called on each thread that is to be covered by the crash handler.
     */
    static void installAlternateStack();
};


#endif

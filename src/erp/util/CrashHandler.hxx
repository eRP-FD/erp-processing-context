/*
 * (C) Copyright IBM Deutschland GmbH 2023
 * (C) Copyright IBM Corp. 2023
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_CRASHHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_CRASHHANDLER_HXX

#include <vector>

/**
 * This class installs a simple signal handler, and when one
 * of these signals is caught, it will print the stack trace
 * and exit the program. Thus, it is mostly useful for signals
 * like SIGSEGV, SIGABRT, etc.
 */
class CrashHandler
{
public:
    static void registerSignalHandlers(const std::vector<int>& signals);
};


#endif

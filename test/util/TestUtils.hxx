/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_UTILS
#define ERP_PROCESSING_CONTEXT_TEST_UTILS

#include <gtest/gtest.h>
#include <thread>

template <typename T>
static void waitFor(T predicate)
{
    for (size_t retries = 0; retries < 100; ++retries)
    {
        if (predicate())
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_TRUE(predicate());
}

#endif // ERP_PROCESSING_CONTEXT_TEST_UTILS

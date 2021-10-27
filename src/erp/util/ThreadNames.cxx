/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/ThreadNames.hxx"

namespace {
    const std::string UnknownThreadName = "unknown-thread";
}



ThreadNames& ThreadNames::instance (void)
{
    static ThreadNames instance;
    return instance;
}


const std::string& ThreadNames::getCurrentThreadName (void)
{
    return getThreadName(std::this_thread::get_id());
}


const std::string& ThreadNames::getThreadName (std::thread::id threadId)
{
    std::lock_guard lock (mMutex);

    const auto candidate = mThreadIdToNameMap.find(threadId);
    if (candidate != mThreadIdToNameMap.end())
        return candidate->second;
    else
        return UnknownThreadName;
}


void ThreadNames::setThreadName (std::thread::id threadId, const std::string& threadName)
{
    std::lock_guard lock (mMutex);

    mThreadIdToNameMap[threadId] = threadName;
}


void ThreadNames::setCurrentThreadName (const std::string& threadName)
{
    setThreadName(std::this_thread::get_id(), threadName);
}


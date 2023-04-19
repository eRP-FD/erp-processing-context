/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/ThreadNames.hxx"
#include "erp/util/TLog.hxx"

ThreadNames& ThreadNames::instance(void)
{
    static ThreadNames instance;
    return instance;
}


std::string ThreadNames::getCurrentThreadName(void)
{
    return getThreadName(std::this_thread::get_id());
}


std::string ThreadNames::getThreadName(std::thread::id threadId)
{
    std::shared_lock lock(mMutex);
    const auto candidate = mThreadIdToNameMap.find(threadId);
    if (candidate == mThreadIdToNameMap.end()) [[unlikely]]
    {
        lock.unlock();
        std::ostringstream oss;
        oss << threadId;
        std::unique_lock ulock(mMutex);
        mThreadIdToNameMap[threadId] = oss.str();
        return oss.str();
    }
    return candidate->second;
}


void ThreadNames::setThreadName(std::thread::id threadId, const std::string& threadName)
{
    TLOG(INFO) << "threadId=" << threadId << " threadName=" << threadName;
    std::unique_lock lock(mMutex);
    mThreadIdToNameMap[threadId] = threadName;
}


void ThreadNames::setCurrentThreadName(const std::string& threadName)
{
    setThreadName(std::this_thread::get_id(), threadName);
}

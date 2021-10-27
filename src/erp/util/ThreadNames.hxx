/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_THREADNAMES_HXX
#define ERP_PROCESSING_CONTEXT_THREADNAMES_HXX

#include <mutex>
#include <unordered_map>
#include <thread>


/**
 * A global map from thread ids to display names. Used for logging.
 */
class ThreadNames
{
public:
    static ThreadNames& instance (void);

    const std::string& getCurrentThreadName (void);
    const std::string& getThreadName (std::thread::id threadId);
    void setThreadName (std::thread::id threadId, const std::string& threadName);
    void setCurrentThreadName (const std::string& threadName);

private:
    mutable std::mutex mMutex;
    std::unordered_map<std::thread::id,std::string> mThreadIdToNameMap;
};


#endif

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include <chrono>
#include <mutex>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

class Configuration;
class PushEventDataCollector;
class PcServiceContext;

class PushEventCreator {
public:
    explicit PushEventCreator(boost::asio::io_context& ioContext);
    void tryPostCreatePushEvent(PushEventDataCollector pushEventData, PcServiceContext& serviceContext);
    /**
     * Wait until all in-flight push-event callbacks have finished and mQueueDepth reaches 0.
     * If the io_context has no running worker threads (e.g. in unit tests) the pending
     * strand work is drained here via poll() so the wait does not block forever.
     */
    void shutdown();

    size_t currentDepth() const;
    size_t discardedSinceWarn() const;
    static std::chrono::seconds warnInterval(const Configuration& config);


private:
    static size_t maxdepth(const Configuration& config);

    boost::asio::io_context& mIoContext;
    boost::asio::strand<boost::asio::io_context::executor_type> mStrand;

    // BEGIN: protected by mQueueMutex
    mutable std::mutex mQueueMutex;
    size_t mDiscarded = 0;
    size_t mQueueDepth = 0;
    std::chrono::steady_clock::time_point mLastWarning{std::chrono::steady_clock::time_point::min()};
    bool mShutdown = false;
    // END: protected by mQueueMutex
};

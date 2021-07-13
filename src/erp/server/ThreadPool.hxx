#ifndef ERP_PROCESSING_CONTEXT_SERVER_THREADPOOL_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_THREADPOOL_HXX

#include "erp/server/Worker.hxx"

#include <boost/asio.hpp>
#include <cstdlib>
#include <thread>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>


namespace boost::asio { class io_context; }


class ThreadPool
{
public:
    ThreadPool (void);
    ~ThreadPool (void);

    void setUp (size_t threadCount);

    boost::asio::io_context& ioContext();
    const boost::asio::io_context& ioContext() const;

    void runWorkerOnThisThread();

    size_t getThreadCount (void) const;

    // the function will be executed on all threads in the pool
    // CAUTION: excessive use might block the server
    void runOnAllThreads(const std::function<void()>& function);
    void joinAllThreads (void);
    void shutDown (void);

    size_t getWorkerCount() const;

private:
    std::vector<std::thread> mThreads;
    boost::asio::io_context mIoContext;

    mutable std::shared_mutex mWorkerListMutex;
    std::list<erp::server::Worker> mWorkerList;
};


#endif

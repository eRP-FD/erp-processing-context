#ifndef ERP_PROCESSING_CONTEXT_WORKER_HXX
#define ERP_PROCESSING_CONTEXT_WORKER_HXX

#include <boost/asio/io_context.hpp>
#include <functional>
#include <list>

namespace erp::server
{

class Worker
{
public:
    Worker() = default;
    void run(boost::asio::io_context& ioContext);

    // run a function on _this_ workers thread
    void queueWork(std::function<void()>&& extraWork);

private:
    void runExtraWork();

    std::mutex mExtraWorkMutex;
    std::list<std::function<void()>> mExtraWork;
};
}


#endif// ERP_PROCESSING_CONTEXT_WORKER_HXX

#include "Worker.hxx"

#include "erp/util/TLog.hxx"
#include "erp/util/TerminationHandler.hxx"

namespace
{
void fatalTermination()
{
    TLOG(ERROR) << ">>>>> caught exception from io_context                                      <<<<<";
    TLOG(ERROR) << ">>>>> This should not happen, the exception should have been caught earlier <<<<<";
    TLOG(ERROR) << ">>>>> shutting down!                                                        <<<<<";
    // We have to shut down the process in this case, because we don't know what is the state of the ioContext.
    // Furthermore leaving the process live with one thread less could eventually end up in a useless process.
    TerminationHandler::instance().notifyTermination(true);
}
}

void erp::server::Worker::run(boost::asio::io_context& ioContext)
{
    try
    {
        while (not ioContext.stopped())
        {
            ioContext.run_one_for(std::chrono::milliseconds(100));
            runExtraWork();
        }
    }
    catch(const std::exception& ex)
    {
        TVLOG(1) << "std::exception::what(): " << ex.what();
        fatalTermination();
    }
    catch(...)
    {
        fatalTermination();
    }
    TVLOG(1) << "finished processing ioContext";
}

void erp::server::Worker::queueWork(std::function<void()>&& extraWork)
{
    std::lock_guard guard{mExtraWorkMutex};
    mExtraWork.emplace_back(std::move(extraWork));
}

void erp::server::Worker::runExtraWork()
{
    std::list<std::function<void()>> extraWorkList;
    { // scope
        std::lock_guard guard{mExtraWorkMutex};
        extraWorkList.swap(mExtraWork);
    }
    for (auto&& extraWork: extraWorkList)
    {
        extraWork();
    }
}

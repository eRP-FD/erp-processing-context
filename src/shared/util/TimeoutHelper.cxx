#include "shared/util/TimeoutHelper.hxx"


void TimeoutHelper::handler(boost::system::error_code ec)
{
    if (! ec.failed())
    {
        mTimedOut = true;
        mSignal.emit(boost::asio::cancellation_type::all);
    }
}

TimeoutHelper::TimeoutHelper(Strand& strand, std::chrono::_V2::steady_clock::duration timeout)
    : mTimer{strand, timeout}
{
    mTimer.async_wait(std::bind_front(&TimeoutHelper::handler, this));
}

void TimeoutHelper::done()
{
    mTimer.cancel();
}

bool TimeoutHelper::timedOut() const
{
    return mTimedOut;
}

TimeoutHelper::operator bool()
{
    return mTimedOut;
}

boost::asio::cancellation_slot TimeoutHelper::slot()
{
    return mSignal.slot();
}

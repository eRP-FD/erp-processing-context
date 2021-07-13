#ifndef ERP_PROCESSING_CONTEXT_CONDITION_HXX
#define ERP_PROCESSING_CONTEXT_CONDITION_HXX

#include <chrono>
#include <condition_variable>
#include <mutex>


/**
 * A simple class that allows the caller to wait on state changes of a value.
 * The intended use is for a state variable which contains an enum value.
 */
template<typename Enum>
class Condition
{
public:
    explicit Condition (Enum initialValue);
    Enum operator() (void) const;
    Condition<Enum>& operator= (Enum newValue);

    /**
     * Block the calling thread until any change has happened.
     * Returns the new value.
     */
    Enum waitForChange (void) const;
    /**
     * Block the calling thread and wait until either the value has the given `valueToWaitFor` value or until `maxWaitTime` has passed.
     * Returns the value at the time of returning so that the caller can determine whether the expected state
     * has been reached.
     */
    Enum waitForValue (const Enum valueToWaitFor, std::chrono::steady_clock::duration maxWaitTime) const;

private:
    Enum mValue;
    mutable std::mutex mMutex;
    mutable std::condition_variable mStateChangeCondition;
};


template<typename Enum>
Condition<Enum>::Condition (Enum initialValue)
    : mValue(initialValue),
      mMutex(),
      mStateChangeCondition()
{
}


template<typename Enum>
Enum Condition<Enum>::operator() (void) const
{
    std::lock_guard lock (mMutex);
    return mValue;
}


template<typename Enum>
Condition<Enum>& Condition<Enum>::operator= (Enum newValue)
{
    std::lock_guard lock (mMutex);
    if (mValue != newValue)
    {
        mValue = newValue;
        mStateChangeCondition.notify_all();
    }
    return *this;
}


template<typename Enum>
Enum Condition<Enum>::waitForChange (void) const
{
    std::unique_lock lock (mMutex);
    mStateChangeCondition.wait(lock);
    return mValue;
}


template<typename Enum>
Enum Condition<Enum>::waitForValue (const Enum valueToWaitFor, const std::chrono::steady_clock::duration maxWaitTime) const
{
    std::unique_lock lock (mMutex);
    mStateChangeCondition.wait_for(
        lock,
        maxWaitTime,
        [valueToWaitFor,this](){return mValue==valueToWaitFor;});
    return mValue;
}

#endif

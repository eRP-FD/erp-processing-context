/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE_MESSAGECOUNTER_HXX
#define EPA_LIBRARY_CRYPTO_TEE_MESSAGECOUNTER_HXX

#include <atomic>
#include <cstdint>
#include <mutex>
namespace epa
{

/**
 * This small wrapper around a `std::uint64_t` value aims at making handling and verification
 * of the message counter as safe and intuitive as possible.
 * It also checks for overflows and throws a `runtime_exception` if one is detected.
 */
class MessageCounter
{
public:
    using ValueType = std::uint64_t;

    /**
     * Initial counter value for the client is 1 (see A_16945-1).
     */
    static MessageCounter createForClient();
    /**
     * Initial counter value for the server is 0 (see A_16952-01).
     */
    static MessageCounter createForServer();

    static MessageCounter createForTest(bool isClient, ValueType initialValue);

    MessageCounter(const MessageCounter& other) noexcept;
    MessageCounter(MessageCounter&& other) noexcept;
    MessageCounter& operator=(const MessageCounter& other) noexcept;
    MessageCounter& operator=(MessageCounter&& other) noexcept;
    ~MessageCounter() = default;

    /**
     * Update can only be made to a `MessageCounter` object via this class, so that race conditions
     * can be avoided for concurrent requests.
     */
    class ExclusiveAccess
    {
    public:
        explicit ExclusiveAccess(MessageCounter& messageCounter);

        ValueType getRequestValue() const;
        ValueType getResponseValue() const;

        /**
         * The client may choose, voluntarily or due to earlier failed requests,
         * to use request counters that are larger then the previous request counter +2.
         * If the server encounters such a counter, it has to adapt its own counter.
         */
        void setRequestCounter(ValueType newValue);

        /**
         * Advance the request counter. The offset is +1 relative to the message counter
         * that is provided by the client, which is usually +1 relative to the previous message
         * counter of the server.
         */
        void advanceToNextRequest();

    private:
        const std::lock_guard<std::mutex> mLock;
        MessageCounter& mMessageCounter;
    };

    ExclusiveAccess getExclusiveAccess();

    ValueType getRequestValue() const;

    /**
     * The response value is not stored explicitly but is computed as `mRequestValue` + 1.
     */
    ValueType getResponseValue() const;

    bool operator==(const MessageCounter& other) const;

    /**
     * Return `value` + `offset` but throw an exception if that would lead to an overflow.
     * @throws TeeProtocol::Exception
     */
    static ValueType guardedAdd(ValueType value, ValueType offset);

private:
#ifdef FRIEND_TEST
    friend class TeeTest;
#endif

    mutable std::mutex mMutex;
    bool mIsClient;
    ValueType mRequestValue;

    MessageCounter(bool isClient, ValueType initialValue);
};

} // namespace epa

#endif

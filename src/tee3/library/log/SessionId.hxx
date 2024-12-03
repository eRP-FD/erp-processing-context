/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_LOG_SESSIONID_HXX
#define EPA_LIBRARY_LOG_SESSIONID_HXX

#include <boost/noncopyable.hpp>
#include <optional>
#include <string>

namespace epa
{

/**
 * The `SessionId` class represents values that can be used to identify a session, or rather the
 * request that is being processed in the session.
 *
 * When a request is being processed then a `SessionId` instance is created for the duration of the
 * session. During this time the static methods of this class can be used to access the session id
 * values, even -- or rather especially -- when the `SessionId` object is not directly available.
 * Note that the `SessionId` object is (indirectly) bound to a thread. Calling e.g. `sessionId()` in
 * different threads returns different values.
 *
 * When there is no request being processed in a thread, you can still call the static methods. The
 * returned values are then `<not-set>`. The `<not-set>` default value was chosen over an empty
 * `std::optional` because the majority of the expected calls for `sessionId()` and
 * `uniqueSessionId()` are expected for log messages. These would have to use a stand-in value for
 * the optional anyway and therefore it is easier to to this directly in the `SessionId` class,
 * rather than at each call site.
 *
 * The values are:
 * - `sessionId` is what motivates the class name. It is the value of the "Session" request header
 *    field.
 * - `uniqueSessionId` is an internally maintained value. In contrast to `sessionId` it is created
 *    when a `SessionId` object is created, while `sessionId` is set up a little later when the
 *    request header has been parsed. The `uniqueSessionId` allows log messages to be identified
 *    to belong to a session when `uniqueSessionId` has not yet been set.
 *
 * In the future the trace-id might be included.
 *
 * Note that at most one SessionId object is allowed per thread at any given time.
 */
class SessionId : private boost::noncopyable
{
public:
    /**
     * Return the SessionId object that is bound to the current thread.
     * When there is no session id bound to the current thread, either because there is no server
     * or no request is being processed in the current thread, a reference to an unbound object is
     * returned, which throws an exception when modified.
     */
    static SessionId& current();

    /**
     * Allow the copying of a SessionId for the use in the TEE protocol encryption.
     * The ServerSessionContext of the outer request is temporarily replaced with a new context
     * for the inner request. The session id should be identical in both cases.
     */
    SessionId(SessionId&& other) noexcept;
    ~SessionId();

    /**
     * Make the called SessionId object the current one in the current thread, i.e. the one that
     * `current()` will return.
     */
    void activate(bool expectNoActiveSessionId = false);

    /**
     * `id` or `index` has not (yet) been set in the current thread.
     */
    static const std::string NotSetValue;

    /**
     * Return the current value of the "Session" request header or `NotSetValue`.
     */
    const std::string& sessionId() const;

    void setSessionId(std::string id);

    /**
     * Return the current value of the unique session id. It is set to a unique identifier when
     * a new session is entered. It does not rely on values that are provided by the request.
     */
    const std::string& uniqueSessionId() const;

    /**
     * Return the current value of the "x-trace-id" request header or `NotSetValue`.
     */
    const std::string& traceId() const;

    void setTraceId(std::string id);


    const std::string& description() const;
    void setDescription(const std::string& description);

protected:
    std::string mSessionId;
    std::string mTraceId;
    const std::string mUniqueSessionId;
    std::string mDescription;
    const bool mThrowOnModification;

    /**
     * mUnboundSessionId is virtually const, because it throws an exception if one of the set
     * methods is called.
     */
    static SessionId mUnboundSessionId;

    friend class ServerSessionContext;
#ifdef FRIEND_TEST
    FRIEND_TEST(JsonLogTest, withSessionId);
    FRIEND_TEST(SessionIdTest, getSetReset);
    FRIEND_TEST(SessionIdTest, multiple);
    FRIEND_TEST(SessionIdTest, multithreaded);
    friend class AccumulatingStopWatchTest;
#endif

    /**
     * Create a new SessionId. When `isBound` is `true` then it is bound to the current thread, i.e.
     * its `mSessionId` and `mUniqueSessionId` values will be returned when calling `sessionIdd()`
     * or `uniqueSessionId()` in the same thread.
     * When `isBound == false` the new object is not bound to the current thread.
     */
    explicit SessionId(bool isBound = true);
    explicit SessionId(std::string&& uniqueSessionId);

    static thread_local SessionId* mCurrentSessionId;
};


/**
 * Can be instantiated on the stack to set a temporary session ID for the current thread. Should
 * only be used for worker (i.e. ThreadPool) threads performing a particular task.
 */
class AdhocSessionId : public SessionId
{
public:
    AdhocSessionId();
};

} // namespace epa
#endif

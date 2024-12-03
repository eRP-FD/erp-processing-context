/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE_TEESESSIONCONTEXT_HXX
#define EPA_LIBRARY_CRYPTO_TEE_TEESESSIONCONTEXT_HXX

#include "library/crypto/tee/MessageCounter.hxx"
#include "library/crypto/tee/TeeContext.hxx"

#include <memory>
#include <string>

namespace epa
{

class TeeContext;


/**
 * Store per-session values in contrast to TeeContext, which stores values which can span multiple sessions.
 * The message counters are usually updated after construction of a `TeeSessionContext` instance. This is due
 * requests and responses being streamed and values, on which the counters are based, become accessible only after
 * construction. The same is true for the additional header values.
 */
class TeeSessionContext
{
public:
    bool hasTeeContext() const;
    TeeContext& teeContext();
    const TeeContext& teeContext() const;
    TeeContext::User teeContextUser;

    MessageCounter::ValueType requestMessageCounter;
    MessageCounter::ValueType responseMessageCounter;

    std::string additionalRequestHeader;
    std::string additionalResponseHeader;

    explicit TeeSessionContext(const std::shared_ptr<TeeContext>& teeContext);
    ~TeeSessionContext();

    TeeSessionContext(const TeeSessionContext&) = delete;
    TeeSessionContext(TeeSessionContext&&) = default;
    TeeSessionContext& operator=(const TeeSessionContext&) = delete;
    TeeSessionContext& operator=(TeeSessionContext&&) = default;

    /**
     * Update both counter values from `teeContext`. Call this method after modifying `teeContext`.
     */
    void setMessageCounter(
        MessageCounter::ValueType newRequestMessageCounter,
        MessageCounter::ValueType newResponseMessageCounter);

    bool isSet() const;

private:
    std::shared_ptr<TeeContext> mTeeContext;
};

} // namespace epa
#endif

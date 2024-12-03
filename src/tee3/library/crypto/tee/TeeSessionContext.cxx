/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee/TeeSessionContext.hxx"
#include "library/crypto/tee/TeeContext.hxx"
#include "library/util/Assert.hxx"

namespace epa
{

TeeSessionContext::TeeSessionContext(const std::shared_ptr<TeeContext>& teeContext)
  : teeContextUser(teeContext ? &*teeContext : nullptr),
    requestMessageCounter(0),
    responseMessageCounter(1),
    additionalRequestHeader(),
    additionalResponseHeader(),
    mTeeContext(teeContext)
{
    if (teeContext != nullptr)
    {
        const auto messageCounter = teeContext->getMessageCounter().getExclusiveAccess();
        setMessageCounter(messageCounter.getRequestValue(), messageCounter.getResponseValue());
    }
}


TeeSessionContext::~TeeSessionContext() = default;


void TeeSessionContext::setMessageCounter(
    const MessageCounter::ValueType newRequestMessageCounter,
    const MessageCounter::ValueType newResponseMessageCounter)
{
    requestMessageCounter = newRequestMessageCounter;
    responseMessageCounter = newResponseMessageCounter;
}


bool TeeSessionContext::isSet() const
{
    return mTeeContext != nullptr && mTeeContext->isSet();
}


bool TeeSessionContext::hasTeeContext() const
{
    return mTeeContext != nullptr;
}


TeeContext& TeeSessionContext::teeContext()
{
    Assert(mTeeContext != nullptr);
    return *mTeeContext;
}


const TeeContext& TeeSessionContext::teeContext() const
{
    Assert(mTeeContext != nullptr);
    return *mTeeContext;
}

} // namespace epa

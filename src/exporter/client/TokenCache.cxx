/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/client/TokenCache.hxx"


TokenCache::TokenCache(TokenType tokenType)
    : mTokenType(tokenType)
{
}

void TokenCache::setToken(std::string_view token, std::chrono::seconds expiresIn)
{
    mToken.assign(token);
    mExpiresAt = std::chrono::steady_clock::now() + expiresIn;
    mIsValid = true;
}


std::string TokenCache::getToken() const
{
    return mToken;
}


bool TokenCache::isValid() const
{
    return mIsValid && std::chrono::steady_clock::now() < mExpiresAt;
}


void TokenCache::invalidate()
{
    mIsValid = false;
    mToken = {};
}

TokenCache::TokenType TokenCache::getType() const
{
    return mTokenType;
}

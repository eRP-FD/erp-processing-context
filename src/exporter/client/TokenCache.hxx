/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include <chrono>


class TokenCache
{
public:
    enum class TokenType : uint8_t
    {
        AccessToken,
        AuthToken
    };

    explicit TokenCache(TokenType tokenType);

    /**
     * @brief Cache given token.
     *
     * @param token The token content
     * @param expiresIn Duration until expiration of the token.
     *
     */
    void setToken(std::string_view token, std::chrono::seconds expiresIn);

    /**
     * @brief Validate token availability and expiration.
     *
     * @returns True when token may be used and false which indicates a new retrieval.
     */
    bool isValid() const;

    /**
     * @brief Discard cached token.
     */
    void invalidate();

    /**
     * @brief Get the token stored in the cache.
     *
     *
     * @returns Token as string
     */
    std::string getToken() const;

    TokenType getType() const;

private:
    TokenType mTokenType;
    std::string mToken;                              //!< Actual token content.
    std::chrono::steady_clock::time_point mExpiresAt;//!< Token expiration date, as received.
    bool mIsValid{false};                            //!< Token validation state.
};

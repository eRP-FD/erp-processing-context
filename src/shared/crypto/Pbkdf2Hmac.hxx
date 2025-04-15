/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_PBKDF2HMAC_HXX
#define ERP_PROCESSING_CONTEXT_PBKDF2HMAC_HXX

#include "shared/util/SafeString.hxx"

#include <vector>

class Pbkdf2Hmac
{
public:
    static constexpr int numberOfIterations = 10'000;
    ///@brief derive a key from input using PDKDF2_HMAC(SHA-256, salt, 10'000) where 10'000 is the number of iterations
    static std::vector<uint8_t> deriveKey(std::string_view input, const SafeString& salt);
};


#endif//ERP_PROCESSING_CONTEXT_PBKDF2HMAC_HXX

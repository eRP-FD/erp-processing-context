/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_RANDOMSOURCE_HXX
#define ERP_PROCESSING_CONTEXT_RANDOMSOURCE_HXX

#include <cstddef>

class SafeString;

/// @brief Genetic interface to access source of random numbers
class RandomSource {
public:
    virtual SafeString randomBytes(size_t count) = 0;
    virtual ~RandomSource() = default;
    static RandomSource& defaultSource();
};


#endif// ERP_PROCESSING_CONTEXT_RANDOMSOURCE_HXX

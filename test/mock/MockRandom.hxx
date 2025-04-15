/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_MOCK_MOCKHSM_HXX
#define ERP_PROCESSING_CONTEXT_TEST_MOCK_MOCKHSM_HXX

#include "shared/crypto/RandomSource.hxx"
#include "shared/util/SafeString.hxx"

class MockRandom
    : public RandomSource
{
public:
    MockRandom (void);

    SafeString randomBytes (size_t count) override;

    void SetRandomBytesOffset(uint8_t offset);

    SafeString operator()(size_t count) const;

private:
    uint8_t mRandomOffset = 0;
};


#endif

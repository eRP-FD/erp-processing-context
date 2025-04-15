/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/mock/MockRandom.hxx"



MockRandom::MockRandom (void) = default;


SafeString MockRandom::randomBytes (const size_t count)
{
    SafeString data{SafeString::no_zero_fill, count};

    // No random bytes for tests. It is better if the data is deterministic and tests are repeatable.
    char* array = data;
    for (size_t index=0; index<count; ++index)
        array[index] = static_cast<char>((index + mRandomOffset) % 256);

    return data;
}

void MockRandom::SetRandomBytesOffset (uint8_t offset)
{
    mRandomOffset = offset;
}

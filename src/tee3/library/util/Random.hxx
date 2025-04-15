/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_RANDOM_HXX
#define EPA_LIBRARY_UTIL_RANDOM_HXX

#include "library/util/BinaryBuffer.hxx"

#include <cstddef>
#include <random>

namespace epa
{

class Random
{
public:
    /**
     * Generates a random int in the interval [a, b)
     *
     * @param fromInclusive  Starting point of the interval
     * @param toExclusive  End point of the interval (exclusive)
     *
     * @return   Random generated int
     */
    template<class Integer>
    static Integer randomIntBetween(Integer fromInclusive, Integer toExclusive);

    /**
     * Generates `count` bytes and returns them in a BinaryBuffer.
     *
     * @param count -- how many bytes to generate
     * @return buffer containing the binary data generated
     */
    static BinaryBuffer randomBinaryData(std::size_t count);

    /**
     * Get a reference to a random generator for the current thread. You can use this freely without
     * mutex locks.
     */
    static std::mt19937& getGeneratorForCurrentThread();
};


template<class Integer>
Integer Random::randomIntBetween(Integer fromInclusive, Integer toExclusive)
{
    std::uniform_int_distribution<Integer> distribution(fromInclusive, toExclusive - 1);
    return distribution(getGeneratorForCurrentThread());
}

} // namespace epa

#endif

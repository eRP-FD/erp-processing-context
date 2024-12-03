/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_UTIL_RANDOM_HXX
#define E_LIBRARY_UTIL_RANDOM_HXX

#include "shared/util/Buffer.hxx"
#include "shared/util/Expect.hxx"

#include <cstddef>
#include <concepts>
#include <iterator>
#include <random>


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
    template<std::integral Integer>
    static Integer randomIntBetween(Integer fromInclusive, Integer toExclusive);

    /**
     * Generates `count` bytes and returns them in an `util::buffer`
     *
     * @param count -- how many bytes to generate
     * @return buffer containing the binary data generated
     */
    static util::Buffer randomBinaryData(std::size_t count);

    /**
     * Get a reference to a random generator for the current thread. You can use this freely without
     * mutex locks.
     */
    static std::mt19937& getGeneratorForCurrentThread();

    /**
     * Select a random element within the range of given iterators
     * [begin, end).
     * @param begin first element that can be selected
     * @param end one element past the last element that can be selected
     * @return iterator to the random element
     */
    template<typename Iterator>
    static Iterator selectRandomElement(Iterator begin, Iterator end) {
        std::advance(begin, randomIntBetween(0L, std::distance(begin, end)));
        return begin;
    }
};

template<std::integral Integer>
Integer Random::randomIntBetween(Integer fromInclusive, Integer toExclusive)
{
    Expect(toExclusive > fromInclusive, "Upper limit must be greater than lower");
    std::uniform_int_distribution<Integer> distribution(fromInclusive, toExclusive - 1);
    return distribution(getGeneratorForCurrentThread());
}

#endif

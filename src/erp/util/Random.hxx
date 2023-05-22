/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_UTIL_RANDOM_HXX
#define E_LIBRARY_UTIL_RANDOM_HXX

#include "erp/util/Buffer.hxx"

#include <cstddef>


class Random
{
public:
    /**
    * Generates a random int in the interval [a, b)
    *
    * @param a  Starting point of the interval
    * @param b  End point of the interval
    *
    * @return   Random generated int
    */
    static int randomIntBetween (int a, int b);

    /**
     * Generates `count` bytes and returns them in an `util::buffer`
     *
     * @param count -- how many bytes to generate
     * @return buffer containing the binary data generated
     */
    static util::Buffer randomBinaryData(std::size_t count);
};

#endif

/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/Random.hxx"
#include "erp/util/Gsl.hxx"
#include "erp/util/OpenSsl.hxx"
#include "erp/util/Expect.hxx"

#include <random>


int Random::randomIntBetween (int a, int b)
{
    Expect(b > a, "Upper limit must be greater than lower");

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distr(a, b - 1);
    return distr(generator);
}


util::Buffer Random::randomBinaryData (std::size_t count)
{
    util::Buffer result(count);
    RAND_bytes(result.data(), gsl::narrow<int>(result.size()));
    return result;
}

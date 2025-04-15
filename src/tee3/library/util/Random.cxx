/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/Random.hxx"
#include "library/wrappers/OpenSsl.hxx"

#include <mutex>

#include "fhirtools/util/Gsl.hxx"

namespace epa
{

BinaryBuffer Random::randomBinaryData(std::size_t count)
{
    BinaryBuffer result(count);
    RAND_bytes(result.data(), gsl::narrow<int>(result.size()));
    return result;
}


std::mt19937& Random::getGeneratorForCurrentThread()
{
    // One generator for each thread.
    static thread_local std::mt19937 generator = []() {
        // One random_device for seeding the generators of all threads.
        static std::random_device rd;
        static std::mutex rdMutex;
        std::scoped_lock lock(rdMutex);
        return std::mt19937(rd());
    }();

    return generator;
}

} // namespace epa

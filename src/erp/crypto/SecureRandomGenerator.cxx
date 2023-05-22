/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/crypto/SecureRandomGenerator.hxx"

#include "erp/util/OpenSsl.hxx"
#include "erp/util/Expect.hxx"

#include <cmath>
#include <stdexcept>


// GEMREQ-start A_19021-02#addEntropy
void SecureRandomGenerator::addEntropy (const SafeString& string)
{
    double entropyBytes = shannonEntropy(string) / 8.0;
    RAND_add(static_cast<const char*>(string), gsl::narrow<int>(string.size()), entropyBytes);
}
// GEMREQ-end A_19021-02#addEntropy


// GEMREQ-start A_19021-02#generate
SafeString SecureRandomGenerator::generate (const std::size_t size)
{
    Expect(size < static_cast<size_t>(std::numeric_limits<int>::max()), "invalid size");

    SafeString result(size);

    if (1 != RAND_priv_bytes(reinterpret_cast<unsigned char*>(static_cast<char*>(result)), static_cast<int>(result.size())))
    {
        Fail2("CSPRNG failure", std::runtime_error);
    }

    return result;
}
// GEMREQ-end A_19021-02#generate


double SecureRandomGenerator::shannonEntropy (const std::string_view& data)
{
    double entropy = 0;

    std::array<uint64_t, 256> countMap{};

    // get occurrence count of each byte
    for (const char ch: data)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        countMap[static_cast<unsigned char>(ch)]++;
    }

    // calculate entropy
    for (const auto& count: countMap)
    {
        if (count > 0)
        {
            double p_of_x = static_cast<double>(count) / gsl::narrow<double>(data.size());
            entropy -= p_of_x * std::log2(p_of_x);
        }
    }

    return entropy * static_cast<double>(data.size());
}

SafeString SecureRandomGenerator::randomBytes(size_t count)
{
    return generate(count);
}

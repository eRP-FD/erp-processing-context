/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee3/Kyber768.hxx"
#include "library/util/Assert.hxx"

#include <botan/kyber.h>
#include <botan/pubkey.h>
#include <botan/system_rng.h>
#include <vector>

namespace epa
{

namespace
{
    constexpr size_t KEM_kyber_768_length_shared_secret = 32;
    constexpr std::string_view kdf = "Raw";
    constexpr auto kyberMode = Botan::KyberMode::Kyber768_R3;


    BinaryBuffer toBinaryBuffer(const std::vector<uint8_t>& data)
    {
        return BinaryBuffer(data.data(), data.size());
    }


    SensitiveDataGuard toSensitiveDataGuard(const Botan::secure_vector<uint8_t>& data)
    {
        return SensitiveDataGuard(data.data(), data.size());
    }

    /**
     * According to https://botan.randombit.net/handbook/api_ref/rng.html#_CPPv421RandomNumberGenerator,
     * the SystemRNG is thread safe.
     * We use the global SystemRNG because that is the one that is reseeded periodically
     * in `EntropyPool::addEntropy()`.
     */
    Botan::RandomNumberGenerator& randomNumberGenerator()
    {
        return Botan::system_rng();
    }
}


bool Kyber768::isValidPublicKey(const BinaryBuffer& publicKey)
{
    Assert(publicKey.size() == 1184)
        << "public Kyber768 key has wrong length, expected 1184, got " << publicKey.size();
    Botan::Kyber_PublicKey botanPublicKey(publicKey, kyberMode);
    auto& rng = randomNumberGenerator();
    return botanPublicKey.check_key(rng, true);
}


std::tuple<BinaryBuffer, SensitiveDataGuard> Kyber768::createKeyPair()
{
    std::vector<uint8_t> salt(16, 0);
    auto& rng = randomNumberGenerator();
    rng.randomize(salt);
    const Botan::Kyber_PrivateKey privateKey(rng, kyberMode);
    const auto serializedPrivateKey = privateKey.private_key_bits();
    const auto serializedPublicKey = privateKey.public_key()->public_key_bits();
    return {
        BinaryBuffer(serializedPublicKey.data(), serializedPublicKey.size()),
        SensitiveDataGuard(serializedPrivateKey.data(), serializedPrivateKey.size())};
}


std::tuple<SensitiveDataGuard, BinaryBuffer> Kyber768::encapsulate(const BinaryBuffer& publicKey)
{
    const Botan::Kyber_PublicKey botanPublicKey(publicKey, kyberMode);
    Botan::PK_KEM_Encryptor enc(botanPublicKey, kdf);
    auto& rng = randomNumberGenerator();
    const std::vector<uint8_t> emptySalt;
    const auto kemResult = enc.encrypt(rng, KEM_kyber_768_length_shared_secret, emptySalt);

    return {
        toSensitiveDataGuard(kemResult.shared_key()),
        toBinaryBuffer(kemResult.encapsulated_shared_key())};
}


SensitiveDataGuard Kyber768::decapsulate(
    const SensitiveDataGuard& privateKey,
    const BinaryBuffer& ciphertext)
{
    Botan::Kyber_PrivateKey botanPrivateKey(*privateKey, kyberMode);
    auto& rng = randomNumberGenerator();
    auto decryptor = Botan::PK_KEM_Decryptor(botanPrivateKey, rng, kdf);
    const std::vector<uint8_t> emptySalt;
    return toSensitiveDataGuard(
        decryptor.decrypt(ciphertext, KEM_kyber_768_length_shared_secret, emptySalt));
}

} // namespace epa

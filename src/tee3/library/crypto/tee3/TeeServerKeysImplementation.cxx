/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee3/TeeServerKeysImplementation.hxx"
#include "library/crypto/SignatureVerifier.hxx"
#include "library/crypto/service/CryptoService.hxx"
#include "library/crypto/tee3/RcaChain.hxx"
#include "library/util/ByteHelper.hxx"
#include "library/util/CertificateVersion.hxx"
#include "library/util/cbor/CborDeserializer.hxx"
#include "library/util/cbor/CborSerializer.hxx"

#include "shared/erp-serverinfo.hxx"
#include "shared/util/Base64.hxx"
#include "shared/tsl/TslProvider.hxx"

namespace epa
{

namespace
{
    /**
     * The version that is recognized for the /Cert.<hash>-<version> endpoint.
     * The hash will be derived from the VAU certificate.
     */
    constexpr uint64_t certificateVersion = 1u;

    /**
     * The maximum lifetime of the signed public keys that are returned by the
     *     /CertData.<hash>-<version>
     * endpoint.
     *
     * gemSpec_Krypt does not define a value. Eventually we will use the maximum lifetime
     * of TEE instances of 1 day.
     * For development we use a larger value of 8 days.
     */
    constexpr auto signedPublicKeysMaxLifetime = std::chrono::hours(8 * 24);

    /**
     * The maximum allowed age of the two key pairs is 30 days (per ANFEPA-2362).
     */
    constexpr auto maxKeyAge = std::chrono::days(30);

    constexpr std::string_view beginCertificate = "-----BEGIN CERTIFICATE-----";
    constexpr std::string_view endCertificate = "-----END CERTIFICATE-----";

    [[maybe_unused]] BinaryBuffer getOcspResponse(const std::string& certificateDerBase64)
    {
        const auto ocspResponseBase64 =
            TslProvider::getInstance().getOcspResponse(certificateDerBase64);
        return BinaryBuffer{Base64::decode(ocspResponseBase64)};
    }

    std::vector<BinaryBuffer> pemChainToDerVector(std::string_view pemChain)
    {
        std::vector<BinaryBuffer> derChain;
        std::string_view::size_type position = 0;
        while (true)
        {
            auto start = pemChain.find(beginCertificate, position);
            if (start == std::string_view::npos)
                break;
            start += beginCertificate.length();
            const auto end = pemChain.find(endCertificate, start);
            Assert(end != std::string_view::npos) << "certificate end not found";
            derChain.emplace_back(Base64::decode(pemChain.substr(start, end - start), true));
            position = end + endCertificate.size();
        }
        return derChain;
    }


    int64_t toSeconds(const std::chrono::system_clock::time_point time)
    {
        return std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
    }
}


TeeServerKeysImplementation::TeeServerKeysImplementation(CryptoService& cryptoService)
  : mEcdhPublicKey(),
    mEcdhPrivateKey(),
    mKyber768PublicKey(),
    mKyber768PrivateKey(),
    mSerializedPublicKeys(),
    mPublicKeySignature(),
    mSigningCertificate(),
    mSigningCertificateHash(),
    mOcspResponse()
{
    createKeyPairs();
    signPublicKeys(cryptoService);
}


Tee3Protocol::PrivateKeys TeeServerKeysImplementation::privateKeys() const
{
    assertKeyLifetimeValid();
    return {.ecdhPrivateKey = mEcdhPrivateKey, .kyber768PrivateKey = mKyber768PrivateKey};
}


Tee3Protocol::SignedPublicKeys TeeServerKeysImplementation::signedPublicKeys() const
{
    assertKeyLifetimeValid();
    return {
        .signedPublicKeys = mSerializedPublicKeys,
        .signatureES256 = mPublicKeySignature,
        .certificateHash = mSigningCertificateHash,
        .cdv = certificateVersion,
        .ocspResponse = mOcspResponse};
}


const Certificate& TeeServerKeysImplementation::signingCertificate() const
{
    assertKeyLifetimeValid();
    Assert(mSigningCertificate != nullptr);
    return *mSigningCertificate;
}


std::optional<Tee3Protocol::AutTeeCertificate> TeeServerKeysImplementation::certificateForHash(
    const BinaryBuffer& hash,
    const uint64_t version) const
{
    assertKeyLifetimeValid();

    // At the moment we only support one certificate. That means that we don't make a lookup
    // with hash and version but only verify that the certificate matches the hash and version.
    if (hash != mSigningCertificateHash)
    {
        TLOG(INFO2) << "hash does not match certificate";
        return std::nullopt;
    }
    else if (version != certificateVersion)
    {
        TLOG(INFO2) << "unsupported certificate version";
        return std::nullopt;
    }
    else
    {
        return Tee3Protocol::AutTeeCertificate{
            .certificate = mSigningCertificateDer,
            .ca = getCaCertificate(),
            .rcaChain = getRcaChain()};
    }
}


void TeeServerKeysImplementation::createKeyPairs()
{
    auto keys = Tee3Protocol::Keypairs::generate();

    mEcdhPublicKey = keys.ecdh.publicKey;
    mEcdhPrivateKey = std::move(keys.ecdh.privateKey);
    mKyber768PublicKey = std::move(keys.kyber768.publicKey);
    mKyber768PrivateKey = std::move(keys.kyber768.privateKey);

    // One could argue that the lifetime of the key pairs is counted from the moment when the
    // keys are exposed to the outside for the first time. But the expected maximum time between
    // pod creation, and therefore key creation, and first key use is probably a couple of hours.
    // Therefore, and in oder to reduce the complexity of the implementation, the clock starts
    // ticking now.
    mKeyCreationTime = std::chrono::system_clock::now();
}


void TeeServerKeysImplementation::signPublicKeys(CryptoService& cryptoService)
{
    using namespace std::string_literals;
    // Creation date in seconds since epoch.
    const auto now = std::chrono::system_clock::now();

    // Serialize the public keys.
    const auto publicKeys = Tee3Protocol::VauKeys{
        .ecdhPublicKey = mEcdhPublicKey,
        .kyber768PublicKey = mKyber768PublicKey,
        .iat = toSeconds(now),
        .exp = toSeconds(now + signedPublicKeysMaxLifetime),
        .comment = "created by IBM TEE, build version "s.append(ErpServerInfo::BuildVersion())
                       .append(", build type ")
                       .append(ErpServerInfo::BuildType())};
    mSerializedPublicKeys = CborSerializer::serialize(publicKeys);

    // Sign the serialized public keys with the AUT private key.
    const auto signature = cryptoService.sign(
        binaryBufferToString(mSerializedPublicKeys),
        CertificateType::C_FD_AUT,
        CryptoService::SignMode::HashAndSign,
        CertificateVersion::Current);
    mPublicKeySignature = dynamic_cast<EcSignature&>(*signature).toRSBuffer(512);

    // Retrieve the AUT certificate from the HSM.
    mSigningCertificate = std::make_unique<Certificate>(
        cryptoService.getSignerCertificate(CertificateType::C_FD_AUT));
    mSigningCertificateDer = BinaryBuffer(mSigningCertificate->toBinaryDer());

    // Store the SHA256 fingerprint of the certificate.
    auto x509 = X509Certificate::createFromX509Pointer(mSigningCertificate->toX509());
    const auto hexFingerprint = x509.getSha256FingerprintHex();
    mSigningCertificateHash = ByteHelper::binaryBufferFromHex(hexFingerprint);

    // Obtain an OCSP response for the certificate.
    //mOcspResponse = getOcspResponse(mSigningCertificate->toDerBase64String()); //TODO
}


void TeeServerKeysImplementation::assertKeyLifetimeValid() const
{
    const auto age = std::chrono::system_clock::now() - mKeyCreationTime;
    Assert(age < maxKeyAge);
}


BinaryBuffer TeeServerKeysProductionHsmImplementation::getCaCertificate() const
{
    // TODO: provide the ca certificate.
    return BinaryBuffer("intermediate certificate based on RCA<N>, N>=5");
}


std::vector<BinaryBuffer> TeeServerKeysProductionHsmImplementation::getRcaChain() const
{
    // Determine the RCA chain of cross signed certificates. The chain is based on RCA5 which is
    // not included because it is the trust anchor that must be already present on the client side.
    // The end of the chain is one of RCA5 (in which case `rcaChain` would be left empty, RCA6, RCA7
    // or RCA8 in their cross-signed forms. The choice depends on `ca`.
    // TODO: for now, use a randomly chosen RCA
    return pemChainToDerVector(RcaChain::getRcaChain(8));
}


BinaryBuffer TeeServerKeysHsmSimulatorImplementation::getCaCertificate() const
{
    return pemChainToDerVector(RcaChain::getTestCa())[0];
}


std::vector<BinaryBuffer> TeeServerKeysHsmSimulatorImplementation::getRcaChain() const
{
    return pemChainToDerVector(RcaChain::getTestRcaChain());
}

} // namespace epa

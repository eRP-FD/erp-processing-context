/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/crypto/AesGcm.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/DiffieHellman.hxx"
#include "erp/crypto/EllipticCurve.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/crypto/SecureRandomGenerator.hxx"
#include "erp/tee/ErpTeeProtocol.hxx"
#include "erp/tee/OuterTeeRequest.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/GLog.hxx"
#include "erp/util/SafeString.hxx"
#include "mock/crypto/MockCryptography.hxx"

#include <fstream>

static std::string usage(const std::string_view& argv0)
{
    std::ostringstream oss;
    oss << "Usage: " << argv0 << " infile outfile\n\n";
    oss << "infile      name of file with plain text request\n";
    oss << "outfile     target file for encrypted request\n";
    return oss.str();
}

static void encrypt(std::ostream& out, const Certificate& vauCertificate, const SafeString& plaintext) {
  // Prepare encryption
    // The key is created in the constructor.
    auto ephemeralKeyPair = EllipticCurve::BrainpoolP256R1->createKeyPair();
    const auto [x,y] = EllipticCurveUtils::getPaddedXYComponents(ephemeralKeyPair, EllipticCurve::KeyCoordinateLength);
    auto dh = EllipticCurve::BrainpoolP256R1->createDiffieHellman();
    dh.setPrivatePublicKey(ephemeralKeyPair);
    dh.setPeerPublicKey(vauCertificate.getPublicKey());
    const auto contentEncryptionKey = SafeString(
        DiffieHellman::hkdf(dh.createSharedKey(), ErpTeeProtocol::keyDerivationInfo, AesGcm128::KeyLength));
    // The IV has been created randomly in the constructor to give tests the opportunity to replace it
    // with a deterministically generated value.
    auto iv = SecureRandomGenerator::generate(AesGcm128::IvLength);
    auto encryptionResult = AesGcm128::encrypt(plaintext, contentEncryptionKey, iv);
    OuterTeeRequest message;
    message.version = '\x01';
    std::copy(x.begin(), x.end(), message.xComponent);
    std::copy(y.begin(), y.end(), message.yComponent);
    const std::string_view ivView = iv;
    std::copy(ivView.begin(), ivView.end(), message.iv);
    message.ciphertext = std::move(encryptionResult.ciphertext);
    std::copy(encryptionResult.authenticationTag.begin(), encryptionResult.authenticationTag.end(), message.authenticationTag);
    out << message.assemble();
}

// for description refer to README.md
int main(int argc, char* argv[])
{
    GLogConfiguration::init_logging("vau_encrypt");
    if (argc != 3)
    {
        LOG(ERROR) << usage(argv[0]);
        return EXIT_FAILURE;
    }
    SafeString infileData {FileHelper::readFileAsString(argv[1])};
    auto vauPublicKey = MockCryptography::getEciesPublicKey();
    const auto& certificate = Certificate::build().withPublicKey(vauPublicKey).build();
    std::ofstream outfile(argv[2], std::ios::binary);
    if (!outfile)
    {
        LOG(ERROR) << "could not open output file";
        return EXIT_FAILURE;
    }
    encrypt(outfile, certificate, infileData);

    return EXIT_SUCCESS;
}

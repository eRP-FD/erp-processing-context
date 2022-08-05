/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

/**
 * Note that this is a test for the client side of the TEE protocol which is not part of the production code tree.
 * But as it is an important part of testing the server side of the TEE protocol and because gematik provided
 * test data in gemSpec_Krypt, lines 3354 to 3380 the client implementation is tested here.
*/

#include "test/mock/ClientTeeProtocol.hxx"

#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/crypto/Jwt.hxx"
#include "erp/tee/OuterTeeRequest.hxx"
#include "erp/util/ByteHelper.hxx"

#include <gtest/gtest.h>


class ClientTeeProtocolTest : public testing::Test
{
public:
    std::string_view toString (const uint8_t* data, const size_t length)
    {
        return std::string_view(reinterpret_cast<const char*>(data), length);
    }
};


/**
 * Test data is based on example in gemspec_Krypt_V2.18, lines 3354 - 3380.
 */
TEST_F(ClientTeeProtocolTest, testSpecExample)//NOLINT(readability-function-cognitive-complexity)
{
    // Create certificate for public ECC key with
    auto vauPublicKey = EllipticCurveUtils::createPublicKeyHex(
        "8634212830dad457ca05305e6687134166b9c21a65ffebf555f4e75dfb048888",
        "66e4b6843624cbda43c97ea89968bc41fd53576f82c03efa7d601b9facac2b29");

    auto [x0,y0] = EllipticCurveUtils::getPublicKeyCoordinatesHex(vauPublicKey);
    ASSERT_EQ(x0, "8634212830dad457ca05305e6687134166b9c21a65ffebf555f4e75dfb048888");
    ASSERT_EQ(y0, "66e4b6843624cbda43c97ea89968bc41fd53576f82c03efa7d601b9facac2b29");

    const auto vauCertificate = Certificate::build()
        .withPublicKey (vauPublicKey)
        .build();

    // Create private ephemeral key
    auto ephemeralPrivateKey = EllipticCurveUtils::createBrainpoolP256R1PrivateKeyHex(
        "5bbba34d47502bd588ed680dfa2309ca375eb7a35ddbbd67cc7f8b6b687a1c1d");

    // That leads to a public key
    auto [x,y] = EllipticCurveUtils::getPublicKeyCoordinatesHex(ephemeralPrivateKey);
    ASSERT_EQ(x, "754e548941e5cd073fed6d734578a484be9f0bbfa1b6fa3168ed7ffb22878f0f");
    ASSERT_EQ(y, "9aef9bbd932a020d8828367bd080a3e72b36c41ee40c87253f9b1b0beb8371bf");

    // ECDH
    DiffieHellman dh = EllipticCurve::BrainpoolP256R1->createDiffieHellman();
    dh.setPrivatePublicKey(ephemeralPrivateKey);
    dh.setPeerPublicKey(vauPublicKey);
    const auto sharedKey = SafeString(dh.createSharedKey());
    const std::string hexSharedKey = ByteHelper::toHex(sharedKey);
    ASSERT_EQ(hexSharedKey, "9656c2b4b3da81d0385f6a1ee60e93b91828fd90231c923d53ce7bbbcd58ceaa");

    // Key derivation with "ecies-vau-transport".
    const std::string derivedKey = DiffieHellman::hkdf(sharedKey, "ecies-vau-transport", 128/8);
    const std::string derivedKeyHex = ByteHelper::toHex(derivedKey);
    ASSERT_EQ(derivedKeyHex, "23977ba552a21363916af9b5147c83d4");

    auto protocol = ClientTeeProtocol();
    protocol.setIv(SafeString(ByteHelper::fromHex("257db4604af8ae0dfced37ce")));
    protocol.setEphemeralKeyPair(ephemeralPrivateKey);
    const std::string requestContentPlaintextRef = "Hallo Test";
    std::string requestContentPlaintext = requestContentPlaintextRef;
    const std::string encryptdRequest = protocol.encryptInnerRequest(
        vauCertificate,
        SafeString(std::move(requestContentPlaintext)));

    // Verify the individual components.
    const auto outerTeeRequest = OuterTeeRequest::disassemble(encryptdRequest);
    EXPECT_EQ(outerTeeRequest.version, 1u);
    EXPECT_EQ(outerTeeRequest.getPublicX(), ByteHelper::fromHex("754e548941e5cd073fed6d734578a484be9f0bbfa1b6fa3168ed7ffb22878f0f"));
    EXPECT_EQ(outerTeeRequest.getPublicY(), ByteHelper::fromHex("9aef9bbd932a020d8828367bd080a3e72b36c41ee40c87253f9b1b0beb8371bf"));
    EXPECT_EQ(outerTeeRequest.getIv(), ByteHelper::fromHex("257db4604af8ae0dfced37ce"));
    EXPECT_EQ(outerTeeRequest.ciphertext.size(), requestContentPlaintextRef.size());
    EXPECT_EQ(outerTeeRequest.ciphertext, ByteHelper::fromHex("86c2b491c7a8309e750b"));
    EXPECT_EQ(outerTeeRequest.getAuthenticationTag(), ByteHelper::fromHex("4e6e307219863938c204dfe85502ee0a"));

    // Verify the complete message.
    ASSERT_EQ(encryptdRequest, ByteHelper::fromHex(
        "01"                                                               // version
        "754e548941e5cd073fed6d734578a484be9f0bbfa1b6fa3168ed7ffb22878f0f" // x-coordinate
        "9aef9bbd932a020d8828367bd080a3e72b36c41ee40c87253f9b1b0beb8371bf" // y-coordinate
        "257db4604af8ae0dfced37ce"                                         // iv
        "86c2b491c7a8309e750b"                                             // encrypted header and body
        "4e6e307219863938c204dfe85502ee0a"                                 // authentication tag
    ));
}


TEST_F(ClientTeeProtocolTest, testCreateRequest)//NOLINT(readability-function-cognitive-complexity)
{
    // Use the public key from the example in gemspec_Krypt#7.2
    auto vauPublicKey = EllipticCurveUtils::createPublicKeyHex(
        "8634212830dad457ca05305e6687134166b9c21a65ffebf555f4e75dfb048888",
        "66e4b6843624cbda43c97ea89968bc41fd53576f82c03efa7d601b9facac2b29");
    const auto vauCertificate = Certificate::build()
        .withPublicKey (vauPublicKey)
        .build();

    const JWT jwt;

    // Create a ClientTeeProtocol ...
    auto protocol = ClientTeeProtocol();
    // ... with an explicitly given ephemeral key pair ...
    protocol.setEphemeralKeyPair(EllipticCurveUtils::createBrainpoolP256R1PrivateKeyHex("5bbba34d47502bd588ed680dfa2309ca375eb7a35ddbbd67cc7f8b6b687a1c1d"));
    // ... and IV.
    const std::string ivRef = "123456789012";
    std::string iv = ivRef;
    protocol.setIv(SafeString(std::move(iv)));

    // Create the wrapped request.
    const std::string serializedInnerRequest = "request-content";
    const std::string wrappedRequest = protocol.createRequest(vauCertificate, serializedInnerRequest, jwt);
    const OuterTeeRequest message = OuterTeeRequest::disassemble(wrappedRequest);

    // Verify the elements of the response.
    ASSERT_EQ(message.version, 1u);
    ASSERT_NE(toString(message.xComponent, 32), toString(message.yComponent, 32));
    ASSERT_EQ(toString(message.xComponent, 32), ByteHelper::fromHex("754e548941e5cd073fed6d734578a484be9f0bbfa1b6fa3168ed7ffb22878f0f"));
    ASSERT_EQ(toString(message.yComponent, 32), ByteHelper::fromHex("9aef9bbd932a020d8828367bd080a3e72b36c41ee40c87253f9b1b0beb8371bf"))
                        << ByteHelper::toHex(toString(message.yComponent, 32));
    ASSERT_EQ(message.ciphertext.size(),
        1 // version
        + jwt.serialize().size()
        + 128ul / 8ul * 2ul                     // requestId, x2 because hex encoded
        + AesGcm128::KeyLength * 2         // AES key,   x2 because hex encoded
        + serializedInnerRequest.size()
        + 4                                // four single space separators
        );
    ASSERT_EQ(toString(message.iv, 12), ivRef);
}

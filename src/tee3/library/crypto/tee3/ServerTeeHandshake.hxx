/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_SERVERTEEHANDSHAKE_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_SERVERTEEHANDSHAKE_HXX

#include "library/crypto/tee3/Tee3Context.hxx"
#include "library/crypto/tee3/TeeHandshakeBase.hxx"
#include "library/util/BinaryBuffer.hxx"

class Header;

namespace epa
{

class TeeServerKeys;


/**
 * Allow the ServerTeeHandshake class to provide virtual functions that help
 * tests to initialize variables with pre-determined values or verify internal results.
 * In order to prevent misuse for production builds, the ServerTeeHandshake class is
 * defined `final` for PU builds, which prevents derived classes to use it as base class and
 * thereby prevents overwriting virtual functions.
 */
#ifdef EPA_BUILD_IS_PU
#define FINAL_FOR_PU final
static constexpr bool isPU = true;
#else
#define FINAL_FOR_PU
static constexpr bool isPU = false;
#endif


/**
 * Receive handshake messages "M1" and "M3" from the client and respond with messages
 * "M2" and "M4", respectively.
 */
class FINAL_FOR_PU ServerTeeHandshake : protected TeeHandshakeBase
{
public:
    explicit ServerTeeHandshake(const TeeServerKeys& serverKeys, bool isPU = epa::isPU);
    ~ServerTeeHandshake() override = default;

    /**
     * Generates VAU-CID value to be used for the TEE channel.
     */
    void generateAndSetVauCid(const Header& header);

    /**
     * Process an M1 message and return an M2 message.
     */
    BinaryBuffer createMessage2(const BinaryBuffer& serializedMessage1);

    /**
     * Process an M3 message and return an M4 message.
     */
    BinaryBuffer createMessage4(const BinaryBuffer& message3);

    /**
     * A_24957: Return AUT-VAU certificate together with the certificate chain
     * that is required for the certificate's validation.
     * This method is intended to be called by the handler for the endpoint
     *
     *     /CertData.<SHA-256-Hash-Value-Hex-[0-9a-f]-Version-Number
     *
     * The returned data is already serialized with CBOR and can be returned as-is in the response
     * body. The response Content-Type must be "application/cbor".
     */
    BinaryBuffer getCertData(); // TODO: Not implemented

    /**
     * Return the Tee3Context that is set up during the TEE handshake.
     * Note that before `createMessage4()` returns, it is only partially initialized.
     */
    Tee3Context context() const;

    void setVauCid(Tee3Protocol::VauCid&& vauCid);
    const Tee3Protocol::VauCid& vauCid() const;
    void setChannelId(std::string&& channelId);
    const std::string& channelId() const;

private:
    const TeeServerKeys& mServerKeys;
    BinaryBuffer mTransscript;
    Tee3Protocol::SymmetricKeys mK1;
    Tee3Protocol::Encapsulation mKemResult1;
    Tee3Protocol::SymmetricKeys mK2KeyConfirmation;
    Tee3Protocol::SymmetricKeys mK2ApplicationData;
    std::optional<KeyId> mKeyId;
    Tee3Protocol::VauCid mVauCid;
    std::string mChannelId;
    bool mIsPU;

#ifdef FRIEND_TEST
    FRIEND_TEST(TeeUserSessionManagerTest, channelUserSessionSetGetRemove);
    FRIEND_TEST(TeeUserSessionManagerTest, authenticationSetGet);
#endif
};

} // namespace epa

#endif

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_HSMMOCKCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_HSMMOCKCLIENT_HXX

#include "erp/hsm/HsmSession.hxx"

#include "erp/crypto/OpenSslHelper.hxx"

#if WITH_HSM_MOCK  != 1
#error HsmMockClient.hxx included but WITH_HSM_MOCK not enabled
#endif

/**
 *  Note that at the moment the mock implementations are stand-ins to verify the builder of the input data.
 *
 */
class HsmMockClient
    : public HsmClient
{
public:
    static constexpr const char* keyDerivationInfo = "ecies-vau-transport";
    static constexpr std::string_view defaultRandomData{"these are not random bytes"};

    HsmMockClient (void);

    ::Nonce getNonce(const ::HsmRawSession& session, uint32_t input) override;

    ErpBlob generatePseudonameKey(const ::HsmRawSession& session, uint32_t input) override;
    ErpArray<Aes256Length> unwrapPseudonameKey(const HsmRawSession& session, UnwrapHashKeyInput&& input) override;

    ErpBlob getTeeToken(
        const HsmRawSession& session,
        TeeTokenRequestInput&& input) override;

    DeriveKeyOutput deriveTaskKey(
        const HsmRawSession& session,
        DeriveKeyInput&& input) override;

    DeriveKeyOutput deriveAuditKey(
        const HsmRawSession& session,
        DeriveKeyInput&& input) override;

    DeriveKeyOutput deriveCommsKey(
        const HsmRawSession& session,
        DeriveKeyInput&& input) override;

    [[nodiscard]] ::DeriveKeyOutput deriveChargeItemKey(const::HsmRawSession& session, ::DeriveKeyInput&& input) override;

    ErpArray<Aes128Length> doVauEcies128(
        const HsmRawSession& session,
        DoVAUECIESInput&& input) override;

    shared_EVP_PKEY getEcPublicKey(const HsmRawSession& session, ErpBlob&& ecKeyPair) override;

    SafeString getVauSigPrivateKey (
        const HsmRawSession& session,
        GetVauSigPrivateKeyInput&& input) override;

    ErpVector getRndBytes(
        const HsmRawSession& session,
        size_t input) override;

    ErpArray<Aes256Length> unwrapHashKey(
        const HsmRawSession& session,
        UnwrapHashKeyInput&& input) override;

    ::ParsedQuote parseQuote(const ::ErpVector& quote) const override;

    ErpBlob wrapRawPayload(
        const HsmRawSession& session,
        WrapRawPayloadInput&& input) override;

    ErpVector unwrapRawPayload(
        const HsmRawSession& session,
        UnwrapRawPayloadInput&& input) override;

    void reconnect (HsmRawSession& session) override;

private:
    ErpBlob mTeeToken;

    // The second parameter is used to create different results for derive...Key() calls
    // in order to simulate the real HSM behaviour.
    DeriveKeyOutput derivePersistenceKey (DeriveKeyInput&& input, ::BlobType expectedBlobType);
    void verifyTeeToken (const ErpBlob& teeToken) const;
};

#endif

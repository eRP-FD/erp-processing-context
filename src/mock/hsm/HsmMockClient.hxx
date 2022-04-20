/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
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

    virtual ErpBlob getTeeToken(
        const HsmRawSession& session,
        TeeTokenRequestInput&& input) override;

    virtual DeriveKeyOutput deriveTaskKey(
        const HsmRawSession& session,
        DeriveKeyInput&& input) override;

    virtual DeriveKeyOutput deriveAuditKey(
        const HsmRawSession& session,
        DeriveKeyInput&& input) override;

    virtual DeriveKeyOutput deriveCommsKey(
        const HsmRawSession& session,
        DeriveKeyInput&& input) override;

    virtual ErpArray<Aes128Length> doVauEcies128(
        const HsmRawSession& session,
        DoVAUECIESInput&& input) override;

    virtual SafeString getVauSigPrivateKey (
        const HsmRawSession& session,
        GetVauSigPrivateKeyInput&& arguments) override;

    virtual ErpVector getRndBytes(
        const HsmRawSession& session,
        size_t input) override;

    virtual ErpArray<Aes256Length> unwrapHashKey(
        const HsmRawSession& session,
        UnwrapHashKeyInput&& input) override;

    ::ParsedQuote parseQuote(const ::ErpVector& quote) const override;

    virtual void reconnect (HsmRawSession& session) override;

private:
    ErpBlob mTeeToken;

    // The second parameter is used to create different results for derive...Key() calls
    // in order to simulate the real HSM behaviour.
    DeriveKeyOutput derivePersistenceKey (DeriveKeyInput&& input, ::BlobType expectedBlobType);
    void verifyTeeToken (const ErpBlob& teeToken) const;
};

#endif

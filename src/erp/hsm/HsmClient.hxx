/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_HSMCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_HSMCLIENT_HXX

#include "erp/hsm/ErpTypes.hxx"

#include "erp/util/SafeString.hxx"

#include <array>
#include <vector>


struct TeeTokenRequestInput
{
    ErpArray<TpmObjectNameLength> akName = {};
    ErpBlob	knownAkBlob;
    ErpBlob	knownQuoteBlob;
    ErpBlob	nonceBlob;
    ErpVector quoteData;
    ErpVector quoteSignature;
};

struct DeriveKeyInput
{
    ErpArray<TpmObjectNameLength> akName = {};
    ErpBlob teeToken;
    ErpBlob derivationKey;
    bool initialDerivation = true;
    ErpVector derivationData;
    BlobId blobId{};
    ::BlobType blobType{};
};

struct OptionalDeriveKeyData
{
    ErpVector salt;
    BlobId blobId{};
};

struct DeriveKeyOutput
{
    ErpArray<Aes256Length> derivedKey = {};
    std::optional<OptionalDeriveKeyData> optionalData;
};

struct DoVAUECIESInput
{
    ErpBlob teeToken;
    ErpBlob eciesKeyPair;
    ErpVector clientPublicKey;
} ;

struct GetVauSigPrivateKeyInput
{
    ErpBlob teeToken;
    ErpBlob vauSigPrivateKey;
};

struct UnwrapHashKeyInput
{
    ErpBlob teeToken;
    ErpBlob key;
};

struct ParsedQuote {
    ::ErpVector qualifiedSignerName;
    ::ErpVector qualifyingInformation;
    ::ErpVector pcrSetFlags;
    ::ErpVector pcrHash;
};

struct Nonce {
    ::ErpVector nonce;
    ::ErpBlob blob;
};

class HsmRawSession;


/**
 * C++ wrapper around the HSM C client interface.
 * This wrapper does not use any of the symbols from the HSM C client so that it can be compiled when the hsmclient
 * library is not available (on Mac). It also allows a mock implementation that does not require a connection to an
 * external HSM.
 *
 * Input and output data structures are as close as possible to the C interface. Names have only been changed to conform
 * to the usual rules (e.g. lowercase first character). Types have been replaced with ErpBlob, ErpArray and ErpVector.
 * The returnCode is not returned. If it is not 0 then an exception is thrown.
 */
class HsmClient
{
public:
    virtual ~HsmClient (void) = default;

    virtual ::Nonce getNonce(const ::HsmRawSession& session, uint32_t input) = 0;

    virtual ErpBlob generatePseudonameKey(const ::HsmRawSession& session, uint32_t input) = 0;
    virtual ErpArray<Aes256Length> unwrapPseudonameKey(const HsmRawSession& session, UnwrapHashKeyInput&& input) = 0;

    virtual ErpBlob getTeeToken(
        const HsmRawSession& session,
        TeeTokenRequestInput&& input) = 0;

    virtual DeriveKeyOutput deriveTaskKey(
        const HsmRawSession& session,
        DeriveKeyInput&& input) = 0;

    virtual DeriveKeyOutput deriveAuditKey(
        const HsmRawSession& session,
        DeriveKeyInput&& input) = 0;

    virtual DeriveKeyOutput deriveCommsKey(
        const HsmRawSession& session,
        DeriveKeyInput&& input) = 0;

    virtual ErpArray<Aes128Length> doVauEcies128(
        const HsmRawSession& session,
        DoVAUECIESInput&& input) = 0;

    [[nodiscard]] virtual::DeriveKeyOutput deriveChargeItemKey(const::HsmRawSession& session, ::DeriveKeyInput&& input) = 0;

    virtual SafeString getVauSigPrivateKey (
        const HsmRawSession& session,
        GetVauSigPrivateKeyInput&& arguments) = 0;

    virtual ErpVector getRndBytes(
        const HsmRawSession& session,
        size_t input) = 0;

    virtual ErpArray<Aes256Length> unwrapHashKey(
        const HsmRawSession& session,
        UnwrapHashKeyInput&& input) = 0;

    virtual ::ParsedQuote parseQuote(const ::ErpVector& quote) const = 0;

    virtual void reconnect (HsmRawSession& session) = 0;
};

#endif

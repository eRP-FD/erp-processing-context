/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_HSMPRODUCTIONCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_HSMPRODUCTIONCLIENT_HXX

#include "shared/hsm/HsmClient.hxx"
#include "shared/hsm/HsmException.hxx"
#include "shared/hsm/HsmEciesCurveMismatchException.hxx"
#include "shared/util/ExceptionWrapper.hxx"


#ifdef __APPLE__
    #error "HSM support is only available for Linux and Windows"
#else
    namespace hsmclient {
    #include <hsmclient/ERP_Client.h>
    #include <hsmclient/ERP_Error.h>
    }
#endif

class HsmIdentity;

constexpr static uint32_t ERP_UTIMACO_FUNCTION_DOESNT_EXIST =     0xB0830006u;
constexpr static uint32_t ERP_UTIMACO_SESSION_EXPIRED =           0xB0830022u;
constexpr static uint32_t ERP_UTIMACO_SIGNATURE_VERIFICATION =    0xB09C0007u;
constexpr static uint32_t ERP_UTIMACO_INVALID_HANDLE =            0xB9000001u;
constexpr static uint32_t ERP_UTIMACO_CONNECTION_TERMIINATED =    0xB9010005u;
constexpr static uint32_t ERP_UTIMACO_TIMEOUT_OCCURED =           0xB9010007u;
constexpr static uint32_t ERP_UTIMACO_INVALID_USER_NAME =         0xB0830007u;
constexpr static uint32_t ERP_UTIMACO_DECRYPT_FAILED =            0xB08B000Eu;
constexpr static uint32_t ERP_UTIMACO_UNKNOWN_USER =              0xB0830016u;
constexpr static uint32_t ERP_UTIMACO_MESSAGING_FAILED =          0xB0830021u;
constexpr static uint32_t ERP_UTIMACO_INVALID_MESSAGING_ID =      0xB0830023u;
constexpr static uint32_t ERP_UTIMACO_MAX_AUTH_FAILURES =         0xB0830046u;
constexpr static uint32_t ERP_UTIMACO_SESSION_CLOSED =            0xb0830072u;
constexpr static uint32_t ERP_UTIMACO_MAX_LOGINS =                0xB083003Eu;


class HsmProductionClient
    : public HsmClient
{
public:
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

    ErpBlob wrapRawPayload(
        const HsmRawSession& session,
        WrapRawPayloadInput&& input) override;

    ErpVector unwrapRawPayload(
        const HsmRawSession& session,
        UnwrapRawPayloadInput&& input) override;

    ErpVector signWithVauAutKey(
        const HsmRawSession& session,
        SignVauAutInput&& input) override;

    ::ParsedQuote parseQuote(const ::ErpVector& quote) const override;

    void reconnect (HsmRawSession& session) override;

    static HsmRawSession connect (const HsmIdentity& identity);
    static void disconnect (HsmRawSession& session);

    static std::string hsmErrorCodeString (const uint32_t errorCode);
    static std::string hsmErrorIndexString (const uint32_t errorCode);
    static std::string hsmErrorDetails (const uint32_t errorCode);
    static std::string hsmErrorMessage (const size_t status, const uint32_t errorCode);
private:
    static HsmRawSession logon(const hsmclient::HSMSession& connectedSession,  const HsmIdentity& identity);
};

#define HsmExpectSuccess(response, message, timer)                                                                     \
    {                                                                                                                  \
        if ((response).returnCode != 0)                                                                                \
        {                                                                                                              \
            std::ostringstream s;                                                                                      \
            s << (message) << ": error " << HsmProductionClient::hsmErrorDetails((response).returnCode);               \
            TVLOG(1) << "throwing HSM exception at " << __FILE__ << ':' << __LINE__ << ": " << s.str();                \
            (timer).notifyFailure(s.str() + " at " + __FILE__ + ":" + std::to_string(__LINE__));                       \
            const auto origin = FileNameAndLineNumber({__FILE__, __LINE__});                                           \
            if ((response).returnCode == ERP_ERR_ECIES_CURVE_MISMATCH)                                                 \
                throw ExceptionWrapper<HsmEciesCurveMismatchException>::create(origin, s.str(), (response).returnCode);\
            else                                                                                                       \
                throw ExceptionWrapper<HsmException>::create(origin, s.str(), (response).returnCode);                  \
        }                                                                                                              \
    }


#endif

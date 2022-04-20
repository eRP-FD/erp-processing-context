/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_HSMPRODUCTIONCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_HSMPRODUCTIONCLIENT_HXX

#include "erp/hsm/HsmClient.hxx"
#include "erp/hsm/HsmException.hxx"
#include "erp/util/ExceptionWrapper.hxx"


#ifdef __APPLE__
    #error "HSM support is only available for Linux and Windows"
#else
    namespace hsmclient {
    #include <hsmclient/ERP_Client.h>
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

    static HsmRawSession connect (const HsmIdentity& identity);
    static void disconnect (HsmRawSession& session);

    static std::string hsmErrorCodeString (const uint32_t errorCode);
    static std::string hsmErrorIndexString (const uint32_t errorCode);
    static std::string hsmErrorDetails (const uint32_t errorCode);
    static std::string hsmErrorMessage (const size_t status, const uint32_t errorCode);
};


#define HsmExpectSuccess(response, message, timer)                                                 \
    if (response.returnCode!=0)                                                                    \
    {                                                                                              \
        std::ostringstream s;                                                                      \
        s << message << ": error " << HsmProductionClient::hsmErrorDetails(response.returnCode);   \
        TVLOG(1) << "throwing HSM exception at " << __FILE__ << ':' << __LINE__ << ": " << s.str();\
        timer.notifyFailure(s.str() + " at " + __FILE__ + ":" + std::to_string(__LINE__));         \
        const auto origin = FileNameAndLineNumber({__FILE__,__LINE__});                            \
        throw ExceptionWrapper<HsmException>::create(origin, s.str(), response.returnCode);        \
    }


#endif

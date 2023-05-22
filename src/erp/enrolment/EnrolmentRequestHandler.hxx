/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ENROLMENTREQUESTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_ENROLMENTREQUESTHANDLER_HXX

#include "erp/pc/PcServiceContext.hxx"
#include "erp/enrolment/EnrolmentModel.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"


using EnrolmentSession = SessionContext;


class EnrolmentRequestHandlerBase
    : public RequestHandlerBasicAuthentication

{
public:
    static constexpr ::std::string_view hashAlgorithm = "SHA256";

    static constexpr std::string_view requestAkName = "/akName";
    static constexpr std::string_view requestBlobData = "/blob/data";
    static constexpr std::string_view requestBlobGeneration = "/blob/generation";
    static constexpr std::string_view requestCertificate = "/certificate";
    static constexpr std::string_view requestCredential = "/credential";
    static constexpr std::string_view requestEndDateTime = "/metadata/endDateTime";
    static constexpr std::string_view requestExpiryDateTime = "/metadata/expiryDateTime";
    static constexpr std::string_view requestHashAlgorithm = "/hashAlgorithm";
    static constexpr std::string_view requestId = "/id";
    static constexpr std::string_view requestNonce = "/nonce";
    static constexpr std::string_view requestPcrSet = "/pcrSet";
    static constexpr std::string_view requestSecret = "/secret";
    static constexpr std::string_view requestStartDateTime = "/metadata/startDateTime";
    static constexpr std::string_view requestValidFrom = "/valid-from";
    static constexpr std::string_view requestVsdm = "/vsdm";

    static constexpr std::string_view responseAkName = "/akName";
    static constexpr std::string_view responseCertificate = "/certificate";
    static constexpr std::string_view responseEkName = "/ekName";
    static constexpr std::string_view responseEnclaveId = "/enclaveId";
    static constexpr std::string_view responseEnclaveTime = "/enclaveTime";
    static constexpr std::string_view responseEnrolmentStatus = "/enrolmentStatus";
    static constexpr std::string_view responsePcrHash = "/pcrHash";
    static constexpr std::string_view responsePcrSet = "/pcrSet";
    static constexpr std::string_view responsePlainTextCredential = "/plainTextCredential";
    static constexpr std::string_view responsePublicKey = "/publicKey";
    static constexpr std::string_view responseQuoteSignature = "/quoteSignature";
    static constexpr std::string_view responseQuotedData = "/quotedData";
    static constexpr std::string_view responseVersionRelease = "/version/release";
    static constexpr std::string_view responseVersionReleasedate = "/version/releasedate";
    static constexpr std::string_view responseVersionBuild = "/version/build";
    static constexpr std::string_view responseVersionBuildType = "/version/buildType";

    /**
     * Not used for the enrolment service. Always returns Operation::UNKNOWN.
     */
    Operation getOperation (void) const override;

    void handleRequest (EnrolmentSession& session) override;

protected:
    virtual EnrolmentModel doHandleRequest (EnrolmentSession& session) = 0;
};


class PutBlobHandler : public EnrolmentRequestHandlerBase
{
protected:
    PutBlobHandler (BlobType blobType, std::string&& endpointDisplayname);

    EnrolmentModel doHandleRequest (EnrolmentSession& session) override;

private:
    const BlobType mBlobType;
    const std::string mEndpointDisplayname;
};

using PostBlobHandler = PutBlobHandler;

class DeleteBlobHandler : public EnrolmentRequestHandlerBase
{
protected:
    explicit DeleteBlobHandler (BlobType type, std::string&& endpointDisplayname);
    EnrolmentModel doHandleRequest (EnrolmentSession& session) override;

private:
    const BlobType mBlobType;
    const std::string mEndpointDisplayname;
};


/**
 * Not all classes of the enrolment service should go into the enrolment namespace.
 * It is used here only to keep the handler names down at a manageable length.
 */
namespace enrolment
{
    class GetEnclaveStatus : public EnrolmentRequestHandlerBase
    {
    protected:
        EnrolmentModel doHandleRequest (EnrolmentSession& session) override;
    };


    /**
     * Get endorsement key from TPM.
     */
    class GetEndorsementKey : public EnrolmentRequestHandlerBase
    {
    protected:
        EnrolmentModel doHandleRequest (EnrolmentSession& session) override;
    };


    /**
     * Get attestation key from TPM.
     */
    class GetAttestationKey : public EnrolmentRequestHandlerBase
    {
    protected:
        EnrolmentModel doHandleRequest (EnrolmentSession& session) override;
    };


    class PostAuthenticationCredential : public EnrolmentRequestHandlerBase
    {
    protected:
        EnrolmentModel doHandleRequest (EnrolmentSession& session) override;
    };


    class PostGetQuote : public EnrolmentRequestHandlerBase
    {
    protected:
        EnrolmentModel doHandleRequest (EnrolmentSession& session) override;
    };


    class PutKnownEndorsementKey : public PutBlobHandler
    {
    public:
        PutKnownEndorsementKey (void);
    };


    class DeleteKnownEndorsementKey : public DeleteBlobHandler
    {
    public:
        DeleteKnownEndorsementKey (void);
    };


    class PutKnownAttestationKey : public EnrolmentRequestHandlerBase
    {
    protected:
        EnrolmentModel doHandleRequest (EnrolmentSession& session) override;
    };


    class DeleteKnownAttestationKey : public DeleteBlobHandler
    {
    public:
        DeleteKnownAttestationKey (void);
    };


    class PutKnownQuote : public EnrolmentRequestHandlerBase
    {
    protected:
        EnrolmentModel doHandleRequest (EnrolmentSession& session) override;
    };


    class DeleteKnownQuote : public DeleteBlobHandler
    {
    public:
        DeleteKnownQuote (void);
    };


    class PutEciesKeypair : public EnrolmentRequestHandlerBase
    {
    protected:
        EnrolmentModel doHandleRequest (EnrolmentSession& session) override;
    };


    class DeleteEciesKeypair : public EnrolmentRequestHandlerBase
    {
    protected:
        EnrolmentModel doHandleRequest (EnrolmentSession& session) override;
    };


    class PutDerivationKey : public EnrolmentRequestHandlerBase
    {
    protected:
        EnrolmentModel doHandleRequest (EnrolmentSession& session) override;
    };


    class DeleteDerivationKey : public EnrolmentRequestHandlerBase
    {
    protected:
        EnrolmentModel doHandleRequest (EnrolmentSession& session) override;
    };

    class PutKvnrHashKey : public PutBlobHandler
    {
    public:
        PutKvnrHashKey (void);
    };

    class PutTelematikIdHashKey : public PutBlobHandler
    {
    public:
        PutTelematikIdHashKey (void);
    };

    class DeleteKvnrHashKey : public DeleteBlobHandler
    {
    public:
        DeleteKvnrHashKey (void);
    };

    class DeleteTelematikIdHashKey : public DeleteBlobHandler
    {
    public:
        DeleteTelematikIdHashKey (void);
    };

class PostVauSig : public PostBlobHandler
{
public:
    PostVauSig();

protected:
    ::EnrolmentModel doHandleRequest(::EnrolmentSession& session) override;
};

class DeleteVauSig : public DeleteBlobHandler
{
public:
    DeleteVauSig();
};

class PostVsdmHmacKey : public EnrolmentRequestHandlerBase
{
protected:
    EnrolmentModel doHandleRequest(EnrolmentSession& session) override;
};

class GetVsdmHmacKey : public EnrolmentRequestHandlerBase
{
protected:
    EnrolmentModel doHandleRequest(EnrolmentSession& session) override;
};

class DeleteVsdmHmacKey : public EnrolmentRequestHandlerBase
{
protected:
    EnrolmentModel doHandleRequest(EnrolmentSession& session) override;
};
} // namespace enrolment


#endif

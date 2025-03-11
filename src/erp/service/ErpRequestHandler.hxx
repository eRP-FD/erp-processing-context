/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_ERPREQUESTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_ERPREQUESTHANDLER_HXX

#include <string>
#include <unordered_set>

#include "shared/network/message/Header.hxx"
#include "shared/network/message/HttpStatus.hxx"
#include "shared/network/message/MimeType.hxx"
#include "shared/fhir/Fhir.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "shared/server/handler/RequestHandlerInterface.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "shared/service/Operation.hxx"
#include "shared/util/Expect.hxx"
#include "shared/validation/JsonValidator.hxx"
#include "shared/validation/XmlValidator.hxx"
#include "fhirtools/util/SaxHandler.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"


class JWT;
class PagingArgument;

class ErpRequestHandler
    : public RequestHandlerInterface
{
public:
    explicit ErpRequestHandler(Operation operation,
                               const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    using OIDsByWorkflow = std::unordered_map<model::PrescriptionType, std::unordered_set<std::string_view>>;
    explicit ErpRequestHandler(Operation operation, OIDsByWorkflow allowedProfessionOiDsByWorkflow);
    ~ErpRequestHandler (void) override = default;

    // Within erp, switch from BaseSessionContext to PcSessionContext via dynamic_cast.
    // All derived classes use SessionContext.

    virtual void handleRequest(PcSessionContext& session) = 0;
    void handleRequest(BaseSessionContext& session) override { handleRequest(dynamic_cast<PcSessionContext&>(session)); }
    virtual void preHandleRequestHook(SessionContext& session);
    void preHandleRequestHook(BaseSessionContext& baseSessionContext) override { preHandleRequestHook(dynamic_cast<SessionContext&>(baseSessionContext)); }

    bool allowedForProfessionOID(std::string_view professionOid,
                                 const std::optional<std::string>& optionalPathIdParameter) const override;
    Operation getOperation (void) const override;
    std::string_view name() const;

    static bool isVerificationIdentityKvnr(const std::string_view& kvnr);

    static bool callerWantsJson (const ServerRequest& request);

protected:
    /// @brief extract and validate ID from URL
    static model::PrescriptionId parseId(const ServerRequest& request, AccessLog& accessLog);

    static void makeResponse(SessionContext& session, HttpStatus status, const model::ResourceBase* body);
    static std::string makeFullUrl(std::string_view tail);
    static std::string getLinkBase ();

    /// @brief parse and validate the request body either using TModel::fromJson or TModel::fromXml based on the provided Content-Type
    template<class TModel>
    [[nodiscard]] static TModel parseAndValidateRequestBody(const SessionContext& context, bool validateGeneric = true);

    static std::optional<std::string> getLanguageFromHeader(const Header& requestHeader);

    static bool responseIsPartOfMultiplePages(const PagingArgument& pagingArgument, std::size_t numOfResults);

    template<class TModel>
    [[nodiscard]] static model::ResourceFactory<TModel> createResourceFactory(const SessionContext& context);
private:
    static void checkValidEncoding(const ContentMimeType& contentMimeType);

    const Operation mOperation;
    std::unordered_set<std::string_view> mAllowedProfessionOIDsAllWorkflows;
    OIDsByWorkflow mAllowedProfessionOIDsByWorkflow;
};

class ErpUnconstrainedRequestHandler : public ErpRequestHandler
{
public:
    explicit ErpUnconstrainedRequestHandler(Operation operation);
    bool allowedForProfessionOID(std::string_view professionOid,
                                 const std::optional<std::string>& optionalPathIdParameter) const override;
};

template<class TModel>
TModel ErpRequestHandler::parseAndValidateRequestBody(const SessionContext& context, bool validateGeneric)
{
    auto resourceFactory = createResourceFactory<TModel>(context);
    if (!validateGeneric)
    {
        resourceFactory.genericValidationMode(model::GenericValidationMode::disable);
    }
    auto profileType = resourceFactory.profileType();
    return std::move(resourceFactory).getValidated(profileType, nullptr);
}


template<class TModel>
[[nodiscard]] model::ResourceFactory<TModel>
ErpRequestHandler::createResourceFactory(const SessionContext& context)
{
    const auto& header = context.request.header();
    const auto& body = context.request.getBody();
    using ResourceFactory = typename model::ResourceFactory<TModel>;
    typename ResourceFactory::Options options;
    auto contentType(header.contentType());
    ErpExpect(contentType.has_value(), HttpStatus::BadRequest, "Missing ContentType in request header");
    ContentMimeType contentMimeType{*contentType};
    checkValidEncoding(contentMimeType);
    const auto& mimeType = contentMimeType.getMimeType();
    if (mimeType == MimeType::fhirJson || mimeType == MimeType::json)
    {
        return ResourceFactory::fromJson(body, context.serviceContext.getJsonValidator(), options);
    }
    else if (mimeType == MimeType::xml || mimeType == MimeType::fhirXml)
    {
        return ResourceFactory::fromXml(body, context.serviceContext.getXmlValidator(), options);
    }
    ErpFail(HttpStatus::UnsupportedMediaType, "Invalid content type (mime type) received: " + header.contentType().value());
}


#endif

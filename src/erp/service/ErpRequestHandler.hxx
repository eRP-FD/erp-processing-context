/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_ERPREQUESTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_ERPREQUESTHANDLER_HXX

#include <string>
#include <unordered_set>

#include "erp/common/Header.hxx"
#include "erp/common/HttpStatus.hxx"
#include "erp/common/MimeType.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/Resource.hxx"
#include "erp/model/ResourceFactory.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/service/Operation.hxx"
#include "erp/util/Expect.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "fhirtools/util/SaxHandler.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"


class JWT;
class PagingArgument;

class ErpRequestHandler
    : public RequestHandlerInterface
{
public:
    explicit ErpRequestHandler (Operation operation, const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    ~ErpRequestHandler (void) override = default;

    void preHandleRequestHook(SessionContext& session) override;

    bool allowedForProfessionOID (std::string_view professionOid) const override;
    Operation getOperation (void) const override;
    std::string_view name() const;

    static bool isVerificationIdentityKvnr(const std::string_view& kvnr);

    static bool callerWantsJson (const ServerRequest& request);

protected:
    void makeResponse(SessionContext& session, HttpStatus status, const model::ResourceBase* body);
    static std::string makeFullUrl(std::string_view tail);
    static std::string getLinkBase ();

    /// @brief parse and validate the request body either using TModel::fromJson or TModel::fromXml based on the provided Content-Type
    template<class TModel>
    [[nodiscard]] static TModel
    parseAndValidateRequestBody(const SessionContext& context, SchemaType schemaType, bool validateGeneric = true);

    static std::optional<std::string> getLanguageFromHeader(const Header& requestHeader);

    static bool responseIsPartOfMultiplePages(const PagingArgument& pagingArgument, std::size_t numOfResults);

    template<class TModel>
    [[nodiscard]] static model::ResourceFactory<TModel> createResourceFactory(const SessionContext& context);
private:

    static void checkValidEncoding(const ContentMimeType& contentMimeType);

    const Operation mOperation;
    std::unordered_set<std::string_view> mAllowedProfessionOIDs;
};

class ErpUnconstrainedRequestHandler : public ErpRequestHandler
{
public:
    explicit ErpUnconstrainedRequestHandler(Operation operation);
    bool allowedForProfessionOID (std::string_view professionOid) const override;
};

template<class TModel>
TModel ErpRequestHandler::parseAndValidateRequestBody(const SessionContext& context, SchemaType schemaType,
                                                      bool validateGeneric)
{
    auto resourceFactory = createResourceFactory<TModel>(context);
    if (!validateGeneric)
    {
        resourceFactory.genericValidationMode(Configuration::GenericValidationMode::disable);
    }
    return std::move(resourceFactory).getValidated(schemaType, context.serviceContext.getXmlValidator(),
                                          context.serviceContext.getInCodeValidator(),
                                          model::ResourceVersion::supportedBundles());
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

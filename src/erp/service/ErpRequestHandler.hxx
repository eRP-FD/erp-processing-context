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
#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/service/Operation.hxx"
#include "erp/util/Expect.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "erp/xml/SaxHandler.hxx"


class JWT;
class PagingArgument;

class ErpRequestHandler
    : public RequestHandlerInterface<PcServiceContext>
{
public:
    explicit ErpRequestHandler (Operation operation, const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    ~ErpRequestHandler (void) override = default;

    void preHandleRequestHook(SessionContext<PcServiceContext>& session) override;

    bool allowedForProfessionOID (std::string_view professionOid) const override;
    Operation getOperation (void) const override;
    std::string_view name() const;

    static bool isVerificationIdentityKvnr(const std::string_view& kvnr);

    static bool callerWantsJson (const ServerRequest& request);

protected:
    void makeResponse(SessionContext<PcServiceContext>& session, HttpStatus status, const model::ResourceBase* body);
    static std::string makeFullUrl(std::string_view tail);
    static std::string getLinkBase ();

    /// @brief parse and validate the request body either using TModel::fromJson or TModel::fromXml based on the provided Content-Type
    template<class TModel>
    [[nodiscard]]
    static TModel parseAndValidateRequestBody(const SessionContext<PcServiceContext>& context, SchemaType schemaType);

    static std::string getLanguageFromHeader(const Header& requestHeader);

    static bool responseIsPartOfMultiplePages(const PagingArgument& pagingArgument, std::size_t numOfResults);

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
TModel ErpRequestHandler::parseAndValidateRequestBody(const SessionContext<PcServiceContext>& context, SchemaType schemaType)
{
    const auto& header = context.request.header();
    const auto& body = context.request.getBody();
    ErpExpect(header.contentType().has_value(), HttpStatus::BadRequest, "Missing ContentType in request header");
    ContentMimeType contentMimeType(header.contentType().value());
    const auto& mimeType = contentMimeType.getMimeType();

    checkValidEncoding(contentMimeType);

    if (mimeType == MimeType::fhirJson || mimeType == MimeType::json)
    {
        auto resource = TModel::fromJson(body, context.serviceContext.getJsonValidator(),
                                         context.serviceContext.getXmlValidator(),
                                         context.serviceContext.getInCodeValidator(), schemaType);
        return resource;
    }
    else if (mimeType == MimeType::xml || mimeType == MimeType::fhirXml)
    {
        auto resource = TModel::fromXml(body, context.serviceContext.getXmlValidator(),
                                        context.serviceContext.getInCodeValidator(), schemaType);
        return resource;
    }

    ErpFail(HttpStatus::UnsupportedMediaType, "Invalid content type (mime type) received: " + header.contentType().value());
}


#endif

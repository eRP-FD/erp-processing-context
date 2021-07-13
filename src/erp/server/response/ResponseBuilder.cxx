#include "erp/server/response/ResponseBuilder.hxx"

#include "erp/model/Bundle.hxx"
#include "erp/util/TLog.hxx"
#include "erp/ErpRequirements.hxx"

#include "erp/common/MimeType.hxx"

#include <unordered_set>


namespace {
}


ResponseBuilder::ResponseBuilder (ServerResponse& response)
    : mResponse(response)
{

}


ResponseBuilder::~ResponseBuilder (void) = default;


ResponseBuilder& ResponseBuilder::status (HttpStatus status)
{
    mResponse.setStatus(status);
    return *this;
}


ResponseBuilder& ResponseBuilder::jsonBody (const std::string& body)
{
    mResponse.setBody(body);
    mResponse.setHeader(Header::ContentType, ContentMimeType::fhirJsonUtf8);
    return *this;
}


ResponseBuilder& ResponseBuilder::xmlBody (const std::string& body)
{
    mResponse.setBody(body);
    mResponse.setHeader(Header::ContentType, ContentMimeType::fhirXmlUtf8);
    return *this;
}


ResponseBuilder& ResponseBuilder::body (const bool useJson, const model::Bundle& bundle)
{
    if (useJson)
        jsonBody(bundle.serializeToJsonString());
    else
        xmlBody(bundle.serializeToXmlString());
    return *this;
}


ResponseBuilder &ResponseBuilder::clearBody(void)
{
    mResponse.setBody("");
    mResponse.removeHeader(Header::ContentType);
    return *this;
}


ResponseBuilder& ResponseBuilder::keepAlive (bool keepAlive)
{
    mResponse.setKeepAlive(keepAlive);
    return *this;
}

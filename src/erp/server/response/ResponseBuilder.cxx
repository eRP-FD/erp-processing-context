/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/server/response/ResponseBuilder.hxx"
#include "erp/model/Resource.hxx"
#include "erp/common/MimeType.hxx"

#include <unordered_set>


namespace {
}


ResponseBuilder::ResponseBuilder (ServerResponse& response)
    : mResponse(response)
{

}


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


ResponseBuilder& ResponseBuilder::body (const bool useJson, const model::ResourceBase& resource)
{
    if (useJson)
        jsonBody(resource.serializeToJsonString());
    else
        xmlBody(resource.serializeToXmlString());
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


ResponseBuilder& ResponseBuilder::header(const std::string& key, const std::string& value)
{
    mResponse.setHeader(key, value);
    return *this;
}

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/server/response/ResponseBuilder.hxx"
#include "shared/model/Resource.hxx"
#include "shared/network/message/MimeType.hxx"

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


ResponseBuilder& ResponseBuilder::fhirJsonBody (const std::string& body)
{
    mResponse.setBody(body);
    mResponse.setHeader(Header::ContentType, ContentMimeType::fhirJsonUtf8);
    return *this;
}


ResponseBuilder& ResponseBuilder::fhirXmlBody (const std::string& body)
{
    mResponse.setBody(body);
    mResponse.setHeader(Header::ContentType, ContentMimeType::fhirXmlUtf8);
    return *this;
}

ResponseBuilder& ResponseBuilder::jsonBody(const std::string& body)
{
    mResponse.setBody(body);
    mResponse.setHeader(Header::ContentType, ContentMimeType::jsonUtf8);
    return *this;
}

ResponseBuilder &ResponseBuilder::clearBody()
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


FhirResponseBuilder& FhirResponseBuilder::body (const bool useJson, const model::ResourceBase& resource)
{
    if (useJson)
    {
        fhirJsonBody(resource.serializeToJsonString());
    }
    else
    {
        fhirXmlBody(resource.serializeToXmlString());
    }
    return *this;
}


/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_RESPONSEBUILDER_HXX
#define ERP_PROCESSING_CONTEXT_RESPONSEBUILDER_HXX


#include "shared/server/response/ServerResponse.hxx"
#include "shared/service/Operation.hxx"


namespace model {class ResourceBase;}


class ResponseBuilder
{
public:
    /**
     * Create a new builder with the response that it will fill in.
     */
    explicit ResponseBuilder (ServerResponse& response);

    ResponseBuilder& status (HttpStatus status);
    ResponseBuilder& fhirJsonBody (const std::string& body);
    ResponseBuilder& fhirXmlBody (const std::string& body);
    ResponseBuilder& jsonBody (const std::string& body);

    ResponseBuilder &clearBody();

    ResponseBuilder &keepAlive(bool keepAlive);

    ResponseBuilder &header(const std::string& key, const std::string& value);

private:
    ServerResponse& mResponse;
};

class FhirResponseBuilder : public ResponseBuilder
{
public:
    using ResponseBuilder::ResponseBuilder;

    FhirResponseBuilder& body(bool useJson, const model::ResourceBase& resource);
};


#endif

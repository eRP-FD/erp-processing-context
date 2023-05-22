/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_RESPONSEBUILDER_HXX
#define ERP_PROCESSING_CONTEXT_RESPONSEBUILDER_HXX


#include "erp/server/response/ServerResponse.hxx"
#include "erp/service/Operation.hxx"


namespace model {class ResourceBase;}


class ResponseBuilder
{
public:
    /**
     * Create a new builder with the response that it will fill in.
     */
    explicit ResponseBuilder (ServerResponse& response);

    ResponseBuilder& status (HttpStatus status);
    ResponseBuilder& jsonBody (const std::string& body);
    ResponseBuilder& xmlBody (const std::string& body);

    ResponseBuilder &body(bool useJson, const model::ResourceBase &resource);

    ResponseBuilder &clearBody(void);

    ResponseBuilder &keepAlive(bool keepAlive);

    ResponseBuilder &header(const std::string& key, const std::string& value);

private:
    ServerResponse& mResponse;
};


#endif

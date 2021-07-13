#ifndef ERP_PROCESSING_CONTEXT_RESPONSEBUILDER_HXX
#define ERP_PROCESSING_CONTEXT_RESPONSEBUILDER_HXX


#include "erp/server/response/ServerResponse.hxx"
#include "erp/service/Operation.hxx"


namespace model {class Bundle;}


class ResponseBuilder
{
public:
    /**
     * Create a new builder with the response that it will fill in.
     */
    explicit ResponseBuilder (ServerResponse& response);
    /**
     * The constructor will validate the values that where give to the builder.
     */
    ~ResponseBuilder (void);

    ResponseBuilder& status (HttpStatus status);
    ResponseBuilder& jsonBody (const std::string& body);
    ResponseBuilder& xmlBody (const std::string& body);
    ResponseBuilder &body(bool useJson, const model::Bundle &bundle);

    ResponseBuilder &clearBody(void);

    ResponseBuilder &keepAlive(bool keepAlive);

private:
    ServerResponse& mResponse;
};


#endif

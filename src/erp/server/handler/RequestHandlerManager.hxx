/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_HANDLER_REQUESTHANDLERMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_HANDLER_REQUESTHANDLERMANAGER_HXX


#include "erp/server/handler/RequestHandlerContext.hxx"

class RequestHandlerManager
{
public:
    using HandlerContext = RequestHandlerContext;
    using HandlerType = typename HandlerContext::HandlerType;

    RequestHandlerContext& addRequestHandler (
        HttpMethod method,
        const std::string& path,
        std::unique_ptr<HandlerType>&& handler);

    void onDeleteDo (const std::string& path, std::unique_ptr<HandlerType>&& handler);
    void onGetDo (const std::string& path, std::unique_ptr<HandlerType>&& handler);
    void onPostDo (const std::string& path, std::unique_ptr<HandlerType>&& handler);
    void onPutDo (const std::string& path, std::unique_ptr<HandlerType>&& handler);
    void onPatchDo (const std::string& path, std::unique_ptr<HandlerType>&& handler);

    struct MatchingHandler
    {
        const HandlerContext* handlerContext = nullptr;
        std::vector<std::string> pathParameters;
        std::vector<std::pair<std::string,std::string>> queryParameters;
        std::string fragment;
    };

    MatchingHandler findMatchingHandler (
        HttpMethod method,
        const std::string& target) const;

private:
    RequestHandlerContainer mRequestHandlers;
};


#endif

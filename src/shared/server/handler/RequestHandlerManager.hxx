/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_HANDLER_REQUESTHANDLERMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_HANDLER_REQUESTHANDLERMANAGER_HXX


#include "shared/server/handler/RequestHandlerContext.hxx"


class Header;
class RequestHandlerManager
{
public:
    using HandlerContext = RequestHandlerContext;
    using HandlerType = typename HandlerContext::HandlerType;

    RequestHandlerContext& addRequestHandler (
        HttpMethod method,
        const std::string& path,
        std::unique_ptr<HandlerType>&& handler);

    RequestHandlerContext& onDeleteDo(const std::string& path, std::unique_ptr<HandlerType>&& handler);
    RequestHandlerContext& onGetDo(const std::string& path, std::unique_ptr<HandlerType>&& handler);
    RequestHandlerContext& onPostDo(const std::string& path, std::unique_ptr<HandlerType>&& handler);
    RequestHandlerContext& onPutDo(const std::string& path, std::unique_ptr<HandlerType>&& handler);
    RequestHandlerContext& onPatchDo(const std::string& path, std::unique_ptr<HandlerType>&& handler);

    struct MatchingHandler
    {
        const HandlerContext* handlerContext = nullptr;
        std::string path;
        std::vector<std::string> pathParameters;
        std::vector<std::pair<std::string,std::string>> queryParameters;
        std::string fragment;
    };

    MatchingHandler findMatchingHandler(const Header& header) const;

    const RequestHandlerContainer& getRequestHandlers() const;

private:
    RequestHandlerContainer mRequestHandlers;
};


#endif

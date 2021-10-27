/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_HANDLER_REQUESTHANDLERMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_HANDLER_REQUESTHANDLERMANAGER_HXX


#include "erp/server/handler/RequestHandlerContext.hxx"

template<class ServiceContextType>
class RequestHandlerManager
{
public:
    using HandlerContext = RequestHandlerContext<ServiceContextType>;
    using HandlerType = typename HandlerContext::HandlerType;

    RequestHandlerContext<ServiceContextType>& addRequestHandler (
        HttpMethod method,
        const std::string& path,
        std::unique_ptr<HandlerType>&& handler);

    void onDeleteDo (const std::string& path, std::unique_ptr<HandlerType>&& action);
    void onGetDo (const std::string& path, std::unique_ptr<HandlerType>&& action);
    void onPostDo (const std::string& path, std::unique_ptr<HandlerType>&& action);
    void onPutDo (const std::string& path, std::unique_ptr<HandlerType>&& action);

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
    RequestHandlerContainer<ServiceContextType> mRequestHandlers;
};


#endif

/*
* (C) Copyright IBM Deutschland GmbH 2021, 2026
* (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
*/

#include "shared/server/handler/RequestHandlerManager.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/util/UrlHelper.hxx"

#ifdef _WINNT_
#ifdef DELETE
#undef DELETE
#endif
#endif


RequestHandlerContext& RequestHandlerManager::addRequestHandler (
   const HttpMethod method,
   const std::string& path,
   std::unique_ptr<HandlerType>&& handler)
{
   Expect( ! path.empty(), "path must not be empty");
   std::string target = UrlHelper::removeTrailingSlash(path);

   return mRequestHandlers.addHandler(
       toString(method) + " " + target,
       method,
       target,
       std::move(handler));
}


RequestHandlerContext& RequestHandlerManager::onDeleteDo(const std::string& path,
                                                         std::unique_ptr<HandlerType>&& handler)
{
    return addRequestHandler(HttpMethod::DELETE, path, std::move(handler));
}


RequestHandlerContext& RequestHandlerManager::onGetDo(const std::string& path, std::unique_ptr<HandlerType>&& handler)
{
    return addRequestHandler(HttpMethod::GET, path, std::move(handler));
}


RequestHandlerContext& RequestHandlerManager::onPostDo(const std::string& path, std::unique_ptr<HandlerType>&& handler)
{
    return addRequestHandler(HttpMethod::POST, path, std::move(handler));
}


RequestHandlerContext& RequestHandlerManager::onPutDo(const std::string& path, std::unique_ptr<HandlerType>&& handler)
{
    return addRequestHandler(HttpMethod::PUT, path, std::move(handler));
}


RequestHandlerContext& RequestHandlerManager::onPatchDo(const std::string& path, std::unique_ptr<HandlerType>&& handler)
{
    return addRequestHandler(HttpMethod::PATCH, path, std::move(handler));
}


RequestHandlerManager::MatchingHandler
RequestHandlerManager::findMatchingHandler(const Header& header) const
{
   // Start with a simple and fast look up.
   auto entry = mRequestHandlers.find(toString(header.method()) + " " + header.target());
   if (entry != mRequestHandlers.end())
   {
       return {entry->second.get(), header.path(), {}, {}, ""};
   }

   // No match. Split the target into path, query and fragment and try to match only the path.
   MatchingHandler result;
   result.path = UrlHelper::removeTrailingSlash(header.path());
   result.queryParameters = UrlHelper::splitQuery(header.query());
   result.fragment = header.fragment();

   // Run a regex match of each handler against the target.
   for (const auto& item : mRequestHandlers)
   {
       auto [matches, parameters] = item.second->matches(header.method(), header.pathOriginal());
       if (matches)
       {
           result.handlerContext = item.second.get();
           result.pathParameters = parameters;
           return result;
       }
   }
    // backwards compatibility: try the url-unescaped path second
    for (const auto& item : mRequestHandlers)
    {
        auto [matches, parameters] = item.second->matches(header.method(), result.path);
        if (matches)
        {
            result.handlerContext = item.second.get();
            result.pathParameters = parameters;
            return result;
        }
    }

   return result; // result.handlerContext is nullptr
}

const RequestHandlerContainer& RequestHandlerManager::getRequestHandlers() const
{
    return mRequestHandlers;
}

/*
* (C) Copyright IBM Deutschland GmbH 2021
* (C) Copyright IBM Corp. 2021
*/

#include "erp/server/handler/RequestHandlerManager.hxx"
#include "erp/server/handler/RequestHandlerContext.hxx"
#include "erp/util/UrlHelper.hxx"

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


void RequestHandlerManager::onDeleteDo (const std::string& path, std::unique_ptr<HandlerType>&& handler)
{
   addRequestHandler(HttpMethod::DELETE, path, std::move(handler));
}


void RequestHandlerManager::onGetDo (const std::string& path, std::unique_ptr<HandlerType>&& handler)
{
   addRequestHandler(HttpMethod::GET, path, std::move(handler));
}


void RequestHandlerManager::onPostDo (const std::string& path, std::unique_ptr<HandlerType>&& handler)
{
   addRequestHandler(HttpMethod::POST, path, std::move(handler));
}


void RequestHandlerManager::onPutDo (const std::string& path, std::unique_ptr<HandlerType>&& handler)
{
   addRequestHandler(HttpMethod::PUT, path, std::move(handler));
}


typename RequestHandlerManager::MatchingHandler RequestHandlerManager::findMatchingHandler (
   const HttpMethod method,
   const std::string& target) const
{
   // Start with a simple and fast look up.
   auto entry = mRequestHandlers.find(toString(method) + " " + target);
   if (entry != mRequestHandlers.end())
       return {entry->second.get(), {}, {}, ""};

   // No match. Split the target into path, query and fragment and try to match only the path.
   auto [path, query, fragment] = UrlHelper::splitTarget(target);
   MatchingHandler result;
   result.queryParameters = UrlHelper::splitQuery(query);
   result.fragment = std::move(fragment);

   // Run a regex match of each handler against the target.
   path = UrlHelper::removeTrailingSlash(path);
   for (const auto& item : mRequestHandlers)
   {
       auto [matches, parameters] = item.second->matches(method, path);
       if (matches)
       {
           result.handlerContext = item.second.get();
           result.pathParameters = parameters;
           return result;
       }
   }

   return {nullptr, {}, {}, ""};
}

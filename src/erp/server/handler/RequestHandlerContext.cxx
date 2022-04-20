/*
* (C) Copyright IBM Deutschland GmbH 2021
* (C) Copyright IBM Corp. 2021
*/

#include "erp/server/handler/RequestHandlerContext.hxx"

#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/util/UrlHelper.hxx"

#include <regex>


RequestHandlerContext::RequestHandlerContext (
   const HttpMethod method_,
   const std::string& path_,
   std::unique_ptr<HandlerType>&& handler_)
   : method(method_),
   path(path_),
   pathRegex(UrlHelper::convertPathToRegex(path_)),
   pathParameterNames(UrlHelper::extractPathParameters(path_)),
   handler(std::move(handler_))
{
}


RequestHandlerContext::~RequestHandlerContext (void) = default;


std::tuple<bool,std::vector<std::string>> RequestHandlerContext::matches (
   const HttpMethod method_,
   const std::string& requestPath) const
{
   if (method != method_)
       return {false,{}};
   else if (pathParameterNames.empty() && requestPath == path)
       return {true,{}};
   else
   {
       std::cmatch result;
       auto matches = std::regex_match(requestPath.c_str(), result, pathRegex);
       if ( ! matches)
           return {false,{}};
       else if (result.empty())
           return {false,{}};
       else if (result.size() != pathParameterNames.size()+1)
           return {false,{}};
       else
       {
           std::vector<std::string> parameterValues;
           parameterValues.reserve(pathParameterNames.size());
           for (size_t index=1; index<result.size(); ++index)
           {
               parameterValues.emplace_back(result.str(index));
           }
           return {true, parameterValues};
       }
   }
}



typename RequestHandlerContainer::ContainerType::const_iterator RequestHandlerContainer::find (const std::string& key) const
{
   return mHandlers.find(key);
}


typename RequestHandlerContainer::ContainerType::const_iterator RequestHandlerContainer::begin (void) const
{
   return mHandlers.begin();
}


typename RequestHandlerContainer::ContainerType::const_iterator RequestHandlerContainer::end (void) const
{
   return mHandlers.end();
}

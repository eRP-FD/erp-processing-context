/*
* (C) Copyright IBM Deutschland GmbH 2021, 2023
* (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
*/

#include "erp/server/handler/RequestHandlerContext.hxx"

#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/util/UrlHelper.hxx"

#include <regex>


RequestHandlerContext::RequestHandlerContext (
   const HttpMethod method,
   const std::string& path,
   std::unique_ptr<HandlerType>&& handler)
   : method(method),
   path(path),
   pathRegex(UrlHelper::convertPathToRegex(path)),
   pathParameterNames(UrlHelper::extractPathParameters(path)),
   handler(std::move(handler))
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
       if (!matches || result.empty() || (result.size() != pathParameterNames.size()+1))
       {
           return {false,{}};
       }
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

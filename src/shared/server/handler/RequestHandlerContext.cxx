/*
* (C) Copyright IBM Deutschland GmbH 2021, 2025
* (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
*/

#include "shared/server/handler/RequestHandlerContext.hxx"

#include "shared/server/handler/RequestHandlerInterface.hxx"
#include "shared/util/UrlHelper.hxx"

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


std::tuple<bool, std::vector<std::string>> RequestHandlerContext::matches(const HttpMethod methodParam,
                                                                          const std::string& requestPath) const
{
    if (method != methodParam)
    {
        return {false, {}};
    }
    if (pathParameterNames.empty() && requestPath == path)
    {
        return {true, {}};
    }
    std::cmatch result;
    auto matches = std::regex_match(requestPath.c_str(), result, pathRegex);
    if (! matches || result.empty() || (result.size() != pathParameterNames.size() + 1))
    {
        return {false, {}};
    }
    std::vector<std::string> parameterValues;
    parameterValues.reserve(pathParameterNames.size());
    for (size_t index = 1; index < result.size(); ++index)
    {
        parameterValues.emplace_back(result.str(index));
    }
    return {true, parameterValues};
}

RequestHandlerContext& RequestHandlerContext::setErpUseCase(ErpUseCaseT&& uc)
{
    mErpUseCase = std::move(uc);
    return *this;
}

bde::UseCase RequestHandlerContext::getErpUseCase(std::optional<model::PrescriptionId> prescriptionId,
                                                  std::optional<std::string_view> professionOid) const
{
    using namespace std::string_literals;
    struct Visitor {
        bde::UseCase operator()(const bde::UseCase& uc) const
        {
            return uc;
        }
        bde::UseCase operator()(const std::map<model::PrescriptionType, bde::UseCase>& uc) const
        {
            Expect(prescriptionId.has_value(),
                   "invalid call to RequestHandlerContext::getErpUseCase without prescriptionId");
            Expect(uc.contains(prescriptionId->type()),
                   "invalid call to RequestHandlerContext::getErpUseCase wit unsupported prescription type "s.append(
                       magic_enum::enum_name(prescriptionId->type())));
            return uc.at(prescriptionId->type());
        }
        bde::UseCase operator()(const std::map<profession_oid::InnerRequestRole, bde::UseCase>& uc) const
        {
            Expect(professionOid.has_value(),
                   "invalid call to RequestHandlerContext::getErpUseCase without professionOid");
            auto innerRole = profession_oid::toInnerRequestRole(*professionOid);
            Expect(uc.contains(innerRole),
                   "invalid call to RequestHandlerContext::getErpUseCase with unsupported professionOid"s.append(
                       *professionOid));
            return uc.at(innerRole);
        }
        bde::UseCase operator()(const bde::NoUseCase&) const
        {
            Fail("invalid call to RequestHandlerContext::getErpUseCase without configured use case");
        }
        std::optional<model::PrescriptionId> prescriptionId;
        std::optional<std::string_view> professionOid;
    };
    const Visitor visitor{.prescriptionId = prescriptionId, .professionOid = professionOid};
    return std::visit(visitor, mErpUseCase);
}

bool RequestHandlerContext::isErpUseCaseSet() const
{
    return ! std::holds_alternative<bde::NoUseCase>(mErpUseCase);
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

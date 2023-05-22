/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/FhirSupplements.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/expression/LiteralExpression.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"

namespace fhirtools
{
Collection PercentResource::eval(const Collection& collection) const
{
    EVAL_TRACE;
    auto context = PercentContext(mFhirStructureRepository).eval(collection);
    auto element = context.singleOrEmpty();
    while (element)
    {
        if (element->isResource())
        {
            return {element};
        }
        element = element->parent();
    }
    return {};
}

Collection PercentRootResource::eval(const Collection& collection) const
{
    EVAL_TRACE;
    auto element = PercentResource(mFhirStructureRepository).eval(collection).singleOrEmpty();
    while (element)
    {
        if (element->isContainerResource())
        {
            return {element};
        }
        element = element->parent();
    }
    return {};
}
ExtensionFunction::ExtensionFunction(const FhirStructureRepository* fhirStructureRepository, ExpressionPtr arg)
    : UnaryExpression(fhirStructureRepository, std::move(arg))
{
    FPExpect(mArg, "missing mandatory argument");
}
Collection ExtensionFunction::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto urlC = mArg->eval(collection);
    if (urlC.empty())
    {
        return {};
    }
    const auto& url = urlC.single()->asString();
    Collection ret;
    for (const auto& item : collection)
    {
        if (! item->definitionPointer().subField("extension"))
        {
            continue;
        }
        const auto extensions = item->subElements("extension");
        for (const auto& extension : extensions)
        {
            if (! extension->definitionPointer().subField("url"))
            {
                continue;
            }
            const Collection urls = extension->subElements("url");
            if (urls.single()->asString() == url)
            {
                ret.push_back(extension);
            }
        }
    }
    return ret;
}

Collection HasValue::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (collection.size() == 1)
    {
        const auto& element = collection.single();
        if (element->hasValue())
        {
            return {makeBoolElement(true)};
        }
    }
    return {makeBoolElement(false)};
}

Collection GetValue::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (HasValue(mFhirStructureRepository).eval(collection).boolean())
    {
        return collection;
    }
    return {};
}

Collection Resolve::eval(const Collection& collection) const
{
    EVAL_TRACE;
    static constexpr std::string_view elementPathIsNotUsed{"n/a"};
    Collection result;
    for (const auto& item : collection)
    {
        const auto& [element, _] = item->resolveReference(elementPathIsNotUsed);
        if (element)
        {
            result.emplace_back(element);
        }
    }
    return result;
}

ConformsTo::ConformsTo(const FhirStructureRepository* fhirStructureRepository, ExpressionPtr arg)
    : UnaryExpression(fhirStructureRepository, std::move(arg))
{
    FPExpect(mArg, "missing mandatory argument");
}
Collection ConformsTo::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (collection.empty())
    {
        return {};
    }
    const auto element = collection.single();
    const auto profile = mArg->eval(collection).single()->asString();
    const auto result = FhirPathValidator::validateWithProfiles(element, element->definitionPointer().element()->name(),
                                                                {profile}, {.validateMetaProfiles = false});
    return {makeBoolElement(result.highestSeverity() < Severity::error)};
}

Collection HtmlChecks::eval(const Collection& collection) const
{
    EVAL_TRACE;
    // Currently covered by xds
    return {makeBoolElement(true)};
}
}

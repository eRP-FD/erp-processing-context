/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/FhirSupplements.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/expression/LiteralExpression.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"

namespace fhirtools
{
EvaluationContext PercentResource::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    auto element = context.context->resourceRoot();
    if (element)
    {
        return context(element);
    }
    return context();
}

EvaluationContext PercentRootResource::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (auto element = context.context->containerResource())
    {
        return context(element);
    }
    return context();
}

ExtensionFunction::ExtensionFunction(
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView, ExpressionPtr arg)
    : UnaryExpression(std::move(fhirStructureRepositoryView), std::move(arg))
{
    FPExpect(mArg, "missing mandatory argument");
}

EvaluationContext ExtensionFunction::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    const auto urlC = mArg->eval(context);
    if (urlC.collection.empty())
    {
        return context();
    }
    const auto& url = urlC.collection.single()->asString();
    auto ret = context();
    for (const auto& item : context.collection)
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
                ret.collection.push_back(extension);
            }
        }
    }
    return ret;
}

EvaluationContext HasValue::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (context.collection.size() == 1)
    {
        const auto& element = context.collection.single();
        if (element->hasValue())
        {
            return context.makeBoolElement(true);
        }
    }
    return context.makeBoolElement(false);
}

EvaluationContext GetValue::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (HasValue(fhirStructureRepository()).eval(context).collection.boolean())
    {
        // return context unchanged
        return context;
    }
    return context();
}

EvaluationContext Resolve::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    static constexpr std::string_view elementPathIsNotUsed{"n/a"};
    auto result = context();
    for (const auto& item : context.collection)
    {
        const auto& [element, _] = item->resolveReference(elementPathIsNotUsed);
        if (element)
        {
            result.collection.emplace_back(element);
        }
    }
    return result;
}

ConformsTo::ConformsTo(std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView,
                       ExpressionPtr arg)
    : UnaryExpression(std::move(fhirStructureRepositoryView), std::move(arg))
{
    FPExpect(mArg, "missing mandatory argument");
}
EvaluationContext ConformsTo::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (context.collection.empty())
    {
        return context();
    }
    const auto element = context.collection.single();
    const auto profile = mArg->eval(context).collection.single()->asString();
    const auto result =
        FhirPathValidator::validateWithProfiles(element, element->definitionPointer().element()->name(),
                                                {DefinitionKey{profile}}, {.validateMetaProfiles = false});
    return context.makeBoolElement(result.highestSeverity() < Severity::error);
}

EvaluationContext HtmlChecks::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    // Currently covered by XSDs
    return context.makeBoolElement(true);
}
}

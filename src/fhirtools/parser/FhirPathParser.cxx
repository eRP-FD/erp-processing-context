/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "fhirtools/parser/FhirPathParser.hxx"

#include <boost/algorithm/string/trim.hpp>
#include <source_location>

#include "antlr4-runtime.h"
#include "fhirpathLexer.h"
#include "fhirpathParser.h"
#include "fhirpathVisitor.h"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/BooleanLogic.hxx"
#include "fhirtools/expression/Comparison.hxx"
#include "fhirtools/expression/Conversion.hxx"
#include "fhirtools/expression/Expression.hxx"
#include "fhirtools/expression/Functions.hxx"
#include "fhirtools/expression/FhirSupplements.hxx"
#include "fhirtools/expression/LiteralExpression.hxx"
#include "fhirtools/expression/StringManipulation.hxx"
#include "fhirtools/model/Element.hxx"

#define TRACE VLOG(3) << std::source_location::current().function_name() << " <<< " << context->getText()

using fhirtools::FhirPathParser;
namespace fhirtools
{

/// @brief implements the ANTLR interface. The functions are called while the expression is parsed by antlr.
///        This class is responsible for compiling each expression into a tree of instances of classes
///        derived from fhirtools::Expression.
/// The callback names correspond to the identifiers in the fhirpath.g4 grammar file.
class FhirPathParser::Impl : public fhirtools::fhirpathVisitor
{
public:
    explicit Impl(const FhirStructureRepository* repository);

    std::any visitIndexerExpression(fhirtools::fhirpathParser::IndexerExpressionContext* context) override;
    std::any visitPolarityExpression(fhirtools::fhirpathParser::PolarityExpressionContext* context) override;
    std::any visitAdditiveExpression(fhirtools::fhirpathParser::AdditiveExpressionContext* context) override;
    std::any
    visitMultiplicativeExpression(fhirtools::fhirpathParser::MultiplicativeExpressionContext* context) override;
    std::any visitUnionExpression(fhirtools::fhirpathParser::UnionExpressionContext* context) override;
    std::any visitOrExpression(fhirtools::fhirpathParser::OrExpressionContext* context) override;
    std::any visitAndExpression(fhirtools::fhirpathParser::AndExpressionContext* context) override;
    std::any visitMembershipExpression(fhirtools::fhirpathParser::MembershipExpressionContext* context) override;
    std::any visitInequalityExpression(fhirtools::fhirpathParser::InequalityExpressionContext* context) override;
    std::any visitInvocationExpression(fhirtools::fhirpathParser::InvocationExpressionContext* context) override;
    std::any visitEqualityExpression(fhirtools::fhirpathParser::EqualityExpressionContext* context) override;
    std::any visitImpliesExpression(fhirtools::fhirpathParser::ImpliesExpressionContext* context) override;
    std::any visitTermExpression(fhirtools::fhirpathParser::TermExpressionContext* context) override;
    std::any visitTypeExpression(fhirtools::fhirpathParser::TypeExpressionContext* context) override;
    std::any visitInvocationTerm(fhirtools::fhirpathParser::InvocationTermContext* context) override;
    std::any visitLiteralTerm(fhirtools::fhirpathParser::LiteralTermContext* context) override;
    std::any visitExternalConstantTerm(fhirtools::fhirpathParser::ExternalConstantTermContext* context) override;
    std::any visitParenthesizedTerm(fhirtools::fhirpathParser::ParenthesizedTermContext* context) override;
    std::any visitNullLiteral(fhirtools::fhirpathParser::NullLiteralContext* context) override;
    std::any visitBooleanLiteral(fhirtools::fhirpathParser::BooleanLiteralContext* context) override;
    std::any visitStringLiteral(fhirtools::fhirpathParser::StringLiteralContext* context) override;
    std::any visitNumberLiteral(fhirtools::fhirpathParser::NumberLiteralContext* context) override;
    std::any visitDateLiteral(fhirtools::fhirpathParser::DateLiteralContext* context) override;
    std::any visitDateTimeLiteral(fhirtools::fhirpathParser::DateTimeLiteralContext* context) override;
    std::any visitTimeLiteral(fhirtools::fhirpathParser::TimeLiteralContext* context) override;
    std::any visitQuantityLiteral(fhirtools::fhirpathParser::QuantityLiteralContext* context) override;
    std::any visitExternalConstant(fhirtools::fhirpathParser::ExternalConstantContext* context) override;
    std::any visitMemberInvocation(fhirtools::fhirpathParser::MemberInvocationContext* context) override;
    std::any visitFunctionInvocation(fhirtools::fhirpathParser::FunctionInvocationContext* context) override;
    std::any visitThisInvocation(fhirtools::fhirpathParser::ThisInvocationContext* context) override;
    std::any visitIndexInvocation(fhirtools::fhirpathParser::IndexInvocationContext* context) override;
    std::any visitTotalInvocation(fhirtools::fhirpathParser::TotalInvocationContext* context) override;
    std::any visitFunction(fhirtools::fhirpathParser::FunctionContext* context) override;
    std::any visitParamList(fhirtools::fhirpathParser::ParamListContext* context) override;
    std::any visitQuantity(fhirtools::fhirpathParser::QuantityContext* context) override;
    std::any visitUnit(fhirtools::fhirpathParser::UnitContext* context) override;
    std::any visitDateTimePrecision(fhirtools::fhirpathParser::DateTimePrecisionContext* context) override;
    std::any visitPluralDateTimePrecision(fhirtools::fhirpathParser::PluralDateTimePrecisionContext* context) override;
    std::any visitTypeSpecifier(fhirtools::fhirpathParser::TypeSpecifierContext* context) override;
    std::any visitQualifiedIdentifier(fhirtools::fhirpathParser::QualifiedIdentifierContext* context) override;
    std::any visitIdentifier(fhirtools::fhirpathParser::IdentifierContext* context) override;

private:
    template<typename TFun>
    std::any fun();
    template<typename TFun>
    std::any unaryFun(fhirtools::fhirpathParser::FunctionContext* context);
    template<typename TFun>
    std::any binaryFun(fhirtools::fhirpathParser::FunctionContext* context);

    template<class TOperator, typename TContext>
    std::any binaryOperator(TContext* context);

    const FhirStructureRepository* mRepository;
};
FhirPathParser::Impl::Impl(const FhirStructureRepository* repository)
    : mRepository(repository)
{
}
template<typename TFun>
std::any FhirPathParser::Impl::fun()
{
    return std::make_any<ExpressionPtr>(std::make_shared<TFun>(mRepository));
}
template<typename TFun>
std::any FhirPathParser::Impl::unaryFun(fhirtools::fhirpathParser::FunctionContext* context)
{
    if (context->paramList())
    {
        FPExpect(context->paramList()->expression().size() == 1,
                 "wrong number of arguments for function " + std::string(TFun::IDENTIFIER));
        return std::make_any<ExpressionPtr>(std::make_shared<TFun>(
            mRepository, std::any_cast<ExpressionPtr>(visit(context->paramList()->expression(0)))));
    }
    else
    {
        return std::make_any<ExpressionPtr>(std::make_shared<TFun>(mRepository, nullptr));
    }
}
template<typename TFun>
std::any FhirPathParser::Impl::binaryFun(fhirtools::fhirpathParser::FunctionContext* context)
{
    if (context->paramList())
    {
        FPExpect(context->paramList()->expression().size() <= 2,
                 "wrong number of arguments for function " + std::string(TFun::IDENTIFIER));
        if (context->paramList()->expression().size() == 2)
        {
            return std::make_any<ExpressionPtr>(std::make_shared<TFun>(
                mRepository, std::any_cast<ExpressionPtr>(visit(context->paramList()->expression(0))),
                std::any_cast<ExpressionPtr>(visit(context->paramList()->expression(1)))));
        }
        else if (context->paramList()->expression().size() == 1)
        {
            return std::make_any<ExpressionPtr>(std::make_shared<TFun>(
                mRepository, std::any_cast<ExpressionPtr>(visit(context->paramList()->expression(0))), nullptr));
        }
    }
    return std::make_any<ExpressionPtr>(std::make_shared<TFun>(mRepository, nullptr, nullptr));
}
template<class TOperator, typename TContext>
std::any FhirPathParser::Impl::binaryOperator(TContext* context)
{
    FPExpect(context->expression().size() == 2,
             "wrong number of arguments for operator " + std::string(TOperator::IDENTIFIER));
    auto lhs = std::any_cast<ExpressionPtr>(visit(context->expression(0)));
    auto rhs = std::any_cast<ExpressionPtr>(visit(context->expression(1)));
    return std::make_any<ExpressionPtr>(std::make_shared<TOperator>(mRepository, lhs, rhs));
}
std::any FhirPathParser::Impl::visitIndexerExpression(fhirtools::fhirpathParser::IndexerExpressionContext* context)
{
    TRACE;
    return binaryOperator<SubsettingIndexer>(context);
}
std::any FhirPathParser::Impl::visitPolarityExpression(fhirtools::fhirpathParser::PolarityExpressionContext* context)
{
    TRACE;
    FPFail("mathematics not implemented");
}
std::any FhirPathParser::Impl::visitAdditiveExpression(fhirtools::fhirpathParser::AdditiveExpressionContext* context)
{
    TRACE;
    FPExpect(context->children.size() == 3, "invalid parse tree");
    const auto op = context->children[1]->getText();
    if (op == "+")
    {
        return binaryOperator<PlusOperator>(context);
    }
    else if (op == "&")
    {
        return binaryOperator<AmpersandOperator>(context);
    }
    FPFail("mathematics not implemented");
}
std::any
FhirPathParser::Impl::visitMultiplicativeExpression(fhirtools::fhirpathParser::MultiplicativeExpressionContext* context)
{
    TRACE;
    FPFail("mathematics not implemented");
}
std::any FhirPathParser::Impl::visitUnionExpression(fhirtools::fhirpathParser::UnionExpressionContext* context)
{
    TRACE;
    return binaryOperator<CombiningUnion>(context);
}
std::any FhirPathParser::Impl::visitOrExpression(fhirtools::fhirpathParser::OrExpressionContext* context)
{
    TRACE;
    FPExpect(context->children.size() == 3, "invalid parse tree");
    const auto op = context->children[1]->getText();
    if (op == "or")
    {
        return binaryOperator<BooleanOrOperator>(context);
    }
    else if (op == "xor")
    {
        return binaryOperator<BooleanXorOperator>(context);
    }
    FPFail("invalid operator: " + op);
}
std::any FhirPathParser::Impl::visitAndExpression(fhirtools::fhirpathParser::AndExpressionContext* context)
{
    TRACE;
    return binaryOperator<BooleanAndOperator>(context);
}
std::any
FhirPathParser::Impl::visitMembershipExpression(fhirtools::fhirpathParser::MembershipExpressionContext* context)
{
    TRACE;
    FPExpect(context->children.size() == 3, "invalid parse tree");
    const auto op = context->children[1]->getText();
    if (op == CollectionsInOperator::IDENTIFIER)
    {
        return binaryOperator<CollectionsInOperator>(context);
    }
    else if (op == CollectionsContainsOperator::IDENTIFIER)
    {
        return binaryOperator<CollectionsContainsOperator>(context);
    }
    FPFail("invalid operator: " + op);
}
std::any
FhirPathParser::Impl::visitInequalityExpression(fhirtools::fhirpathParser::InequalityExpressionContext* context)
{
    TRACE;
    FPExpect(context->children.size() == 3, "invalid parse tree");
    const auto op = context->children[1]->getText();
    if (op == ComparisonOperatorLessOrEqual::IDENTIFIER)
    {
        return binaryOperator<ComparisonOperatorLessOrEqual>(context);
    }
    else if (op == ComparisonOperatorLessThan::IDENTIFIER)
    {
        return binaryOperator<ComparisonOperatorLessThan>(context);
    }
    else if (op == ComparisonOperatorGreaterThan::IDENTIFIER)
    {
        return binaryOperator<ComparisonOperatorGreaterThan>(context);
    }
    else if (op == ComparisonOperatorGreaterOrEqual::IDENTIFIER)
    {
        return binaryOperator<ComparisonOperatorGreaterOrEqual>(context);
    }
    FPFail("invalid operator: " + op);
}
std::any
FhirPathParser::Impl::visitInvocationExpression(fhirtools::fhirpathParser::InvocationExpressionContext* context)
{
    TRACE;
    auto invocationExpression = std::any_cast<ExpressionPtr>(visit(context->invocation()));
    auto expressionExpression = std::any_cast<ExpressionPtr>(visit(context->expression()));
    return std::make_any<ExpressionPtr>(
        std::make_shared<InvocationExpression>(mRepository, expressionExpression, invocationExpression));
}
std::any FhirPathParser::Impl::visitEqualityExpression(fhirtools::fhirpathParser::EqualityExpressionContext* context)
{
    TRACE;
    FPExpect(context->children.size() == 3, "invalid parse tree");
    const auto op = context->children[1]->getText();
    if (op == EqualityEqualsOperator::IDENTIFIER)
    {
        return binaryOperator<EqualityEqualsOperator>(context);
    }
    else if (op == EqualityNotEqualsOperator::IDENTIFIER)
    {
        return binaryOperator<EqualityNotEqualsOperator>(context);
    }
    else if (op == EqualityEquivalentOperator::IDENTIFIER)
    {
        FPFail("not implemented: " + op);
    }
    else if (op == EqualityNotEquivalentOperator::IDENTIFIER)
    {
        FPFail("not implemented: " + op);
    }
    FPFail("invalid operator: " + op);
}
std::any FhirPathParser::Impl::visitImpliesExpression(fhirtools::fhirpathParser::ImpliesExpressionContext* context)
{
    TRACE;
    return binaryOperator<BooleanImpliesOperator>(context);
}
std::any FhirPathParser::Impl::visitTermExpression(fhirtools::fhirpathParser::TermExpressionContext* context)
{
    TRACE;
    return visit(context->term());
}
std::any FhirPathParser::Impl::visitTypeExpression(fhirtools::fhirpathParser::TypeExpressionContext* context)
{
    TRACE;
    FPExpect(context->children.size() == 3, "invalid parse tree");
    const auto op = context->children[1]->getText();
    if (op == TypesIsOperator::IDENTIFIER)
    {
        return std::make_any<ExpressionPtr>(
            std::make_shared<TypesIsOperator>(mRepository, std::any_cast<ExpressionPtr>(visit(context->expression())),
                                              std::any_cast<std::string>(visit(context->typeSpecifier()))));
    }
    if (op == TypeAsOperator::IDENTIFIER)
    {
        return std::make_any<ExpressionPtr>(
            std::make_shared<TypeAsOperator>(mRepository, std::any_cast<ExpressionPtr>(visit(context->expression())),
                                             std::any_cast<std::string>(visit(context->typeSpecifier()))));
    }
    FPFail("invalid operator: " + op);
}
std::any FhirPathParser::Impl::visitInvocationTerm(fhirtools::fhirpathParser::InvocationTermContext* context)
{
    TRACE;
    return visit(context->invocation());
}
std::any FhirPathParser::Impl::visitLiteralTerm(fhirtools::fhirpathParser::LiteralTermContext* context)
{
    TRACE;
    return visit(context->literal());
}
std::any
FhirPathParser::Impl::visitExternalConstantTerm(fhirtools::fhirpathParser::ExternalConstantTermContext* context)
{
    TRACE;
    return visit(context->externalConstant());
}
std::any FhirPathParser::Impl::visitParenthesizedTerm(fhirtools::fhirpathParser::ParenthesizedTermContext* context)
{
    TRACE;
    return visit(context->expression());
}
std::any FhirPathParser::Impl::visitNullLiteral(fhirtools::fhirpathParser::NullLiteralContext* context)
{
    TRACE;
    return std::make_any<ExpressionPtr>(std::make_shared<LiteralNullExpression>(mRepository));
}
std::any FhirPathParser::Impl::visitBooleanLiteral(fhirtools::fhirpathParser::BooleanLiteralContext* context)
{
    TRACE;
    if (context->getText() == "true")
    {
        return std::make_any<ExpressionPtr>(std::make_shared<LiteralBooleanExpression>(mRepository, true));
    }
    return std::make_any<ExpressionPtr>(std::make_shared<LiteralBooleanExpression>(mRepository, false));
}
std::any FhirPathParser::Impl::visitStringLiteral(fhirtools::fhirpathParser::StringLiteralContext* context)
{
    TRACE;
    auto str = context->STRING()->getText();
    FPExpect(str.length() >= 2, "invalid string literal " + str);
    boost::trim_if(str, [](const auto& c) {
        return c == '\'';
    });
    return std::make_any<ExpressionPtr>(std::make_shared<LiteralStringExpression>(mRepository, str));
}
std::any FhirPathParser::Impl::visitNumberLiteral(fhirtools::fhirpathParser::NumberLiteralContext* context)
{
    TRACE;
    const auto& str = context->NUMBER()->getText();
    if (str.find('.') == std::string::npos)
    {
        return std::make_any<ExpressionPtr>(std::make_shared<LiteralIntegerExpression>(mRepository, std::stoi(str)));
    }
    return std::make_any<ExpressionPtr>(
        std::make_shared<LiteralDecimalExpression>(mRepository, Element::DecimalType(str)));
}
std::any FhirPathParser::Impl::visitDateLiteral(fhirtools::fhirpathParser::DateLiteralContext* context)
{
    TRACE;
    return std::make_any<ExpressionPtr>(
        std::make_shared<LiteralDateExpression>(mRepository, context->DATE()->getText()));
}
std::any FhirPathParser::Impl::visitDateTimeLiteral(fhirtools::fhirpathParser::DateTimeLiteralContext* context)
{
    TRACE;
    return std::make_any<ExpressionPtr>(
        std::make_shared<LiteralDateTimeExpression>(mRepository, context->DATETIME()->getText()));
}
std::any FhirPathParser::Impl::visitTimeLiteral(fhirtools::fhirpathParser::TimeLiteralContext* context)
{
    TRACE;
    return std::make_any<ExpressionPtr>(
        std::make_shared<LiteralTimeExpression>(mRepository, context->TIME()->getText()));
}
std::any FhirPathParser::Impl::visitQuantityLiteral(fhirtools::fhirpathParser::QuantityLiteralContext* context)
{
    TRACE;
    return visit(context->quantity());
}
std::any FhirPathParser::Impl::visitExternalConstant(fhirtools::fhirpathParser::ExternalConstantContext* context)
{
    TRACE;
    std::string variable;
    if (context->STRING())
    {
        variable = boost::trim_copy_if(context->STRING()->getText(), [](const auto& c) {
            return c == '\'';
        });
    }
    else if (context->identifier())
    {
        variable = std::any_cast<std::string>(visit(context->identifier()));
    }
    if (variable == "ucum")
    {
        return std::make_any<ExpressionPtr>(
            std::make_shared<LiteralStringExpression>(mRepository, "http://unitsofmeasure.org"));
    }
    else if (variable == "context")
    {
        return std::make_any<ExpressionPtr>(std::make_shared<PercentContext>(mRepository));
    }
    else if (variable == "resource")
    {
        return std::make_any<ExpressionPtr>(std::make_shared<PercentResource>(mRepository));
    }
    else if (variable == "rootResource")
    {
        return std::make_any<ExpressionPtr>(std::make_shared<PercentRootResource>(mRepository));
    }
    FPFail("external constant not implemented: " + variable);
}
std::any FhirPathParser::Impl::visitMemberInvocation(fhirtools::fhirpathParser::MemberInvocationContext* context)
{
    TRACE;
    const auto identifier = std::any_cast<std::string>(visit(context->identifier()));
    return std::make_any<ExpressionPtr>(std::make_shared<PathSelection>(mRepository, identifier));
}
std::any FhirPathParser::Impl::visitFunctionInvocation(fhirtools::fhirpathParser::FunctionInvocationContext* context)
{
    TRACE;
    return visit(context->function());
}
std::any FhirPathParser::Impl::visitThisInvocation(fhirtools::fhirpathParser::ThisInvocationContext* context)
{
    TRACE;
    return std::make_any<ExpressionPtr>(std::make_shared<DollarThis>(mRepository));
}
std::any FhirPathParser::Impl::visitIndexInvocation(fhirtools::fhirpathParser::IndexInvocationContext* context)
{
    TRACE;
    FPFail("not implemented: $index");
}
std::any FhirPathParser::Impl::visitTotalInvocation(fhirtools::fhirpathParser::TotalInvocationContext* context)
{
    TRACE;
    FPFail("not implemented: $total");
}
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::any FhirPathParser::Impl::visitFunction(fhirtools::fhirpathParser::FunctionContext* context)
{
    TRACE;
    auto identifier = std::any_cast<std::string>(visit(context->identifier()));
    if (identifier == ExistenceExists::IDENTIFIER)
    {
        return unaryFun<ExistenceExists>(context);
    }
    else if (identifier == ExistenceEmpty::IDENTIFIER)
    {
        return fun<ExistenceEmpty>();
    }
    else if (identifier == ExistenceAll::IDENTIFIER)
    {
        return unaryFun<ExistenceAll>(context);
    }
    else if (identifier == ExistenceAllTrue::IDENTIFIER)
    {
        return fun<ExistenceAllTrue>();
    }
    else if (identifier == ExistenceAnyTrue::IDENTIFIER)
    {
        return fun<ExistenceAnyTrue>();
    }
    else if (identifier == ExistenceAllFalse::IDENTIFIER)
    {
        return fun<ExistenceAllFalse>();
    }
    else if (identifier == ExistenceAnyFalse::IDENTIFIER)
    {
        return fun<ExistenceAnyFalse>();
    }
    else if (identifier == ExistenceCount::IDENTIFIER)
    {
        return fun<ExistenceCount>();
    }
    else if (identifier == ExistenceDistinct::IDENTIFIER)
    {
        return fun<ExistenceDistinct>();
    }
    else if (identifier == ExistenceIsDistinct::IDENTIFIER)
    {
        return fun<ExistenceIsDistinct>();
    }
    else if (identifier == FilteringWhere::IDENTIFIER)
    {
        return unaryFun<FilteringWhere>(context);
    }
    else if (identifier == FilteringSelect::IDENTIFIER)
    {
        return unaryFun<FilteringSelect>(context);
    }
    else if (identifier == FilteringOfType::IDENTIFIER)
    {
        return unaryFun<FilteringOfType>(context);
    }
    else if (identifier == SubsettingFirst::IDENTIFIER)
    {
        return fun<SubsettingFirst>();
    }
    else if (identifier == SubsettingTail::IDENTIFIER)
    {
        return fun<SubsettingTail>();
    }
    else if (identifier == SubsettingIntersect::IDENTIFIER)
    {
        return unaryFun<SubsettingIntersect>(context);
    }
    else if (identifier == CombiningUnion::IDENTIFIER)
    {
        return binaryFun<CombiningUnion>(context);
    }
    else if (identifier == CombiningCombine::IDENTIFIER)
    {
        return unaryFun<CombiningCombine>(context);
    }
    else if (identifier == ConversionIif::IDENTIFIER)
    {
        FPExpect(context->paramList() &&
                     (context->paramList()->expression().size() == 2 || context->paramList()->expression().size() == 3),
                 "wrong number of arguments for iif");
        auto thirdArg = context->paramList()->expression().size() == 3
                            ? std::any_cast<ExpressionPtr>(visit(context->paramList()->expression(2)))
                            : nullptr;
        return std::make_any<ExpressionPtr>(std::make_shared<ConversionIif>(
            mRepository, std::any_cast<ExpressionPtr>(visit(context->paramList()->expression(0))),
            std::any_cast<ExpressionPtr>(visit(context->paramList()->expression(1))), thirdArg));
    }
    else if (identifier == ConversionToString::IDENTIFIER)
    {
        return fun<ConversionToString>();
    }
    else if (identifier == ConversionToInteger::IDENTIFIER)
    {
        return fun<ConversionToInteger>();
    }
    else if (identifier == StringManipIndexOf::IDENTIFIER)
    {
        return unaryFun<StringManipIndexOf>(context);
    }
    else if (identifier == StringManipSubstring::IDENTIFIER)
    {
        return binaryFun<StringManipSubstring>(context);
    }
    else if (identifier == StringManipStartsWith::IDENTIFIER)
    {
        return unaryFun<StringManipStartsWith>(context);
    }
    else if (identifier == StringManipContains::IDENTIFIER)
    {
        return unaryFun<StringManipContains>(context);
    }
    else if (identifier == StringManipMatches::IDENTIFIER)
    {
        return unaryFun<StringManipMatches>(context);
    }
    else if (identifier == StringManipReplaceMatches::IDENTIFIER)
    {
        return binaryFun<StringManipReplaceMatches>(context);
    }
    else if (identifier == StringManipLength::IDENTIFIER)
    {
        return fun<StringManipLength>();
    }
    else if (identifier == TreeNavigationChildren::IDENTIFIER)
    {
        return fun<TreeNavigationChildren>();
    }
    else if (identifier == TreeNavigationDescendants::IDENTIFIER)
    {
        return fun<TreeNavigationDescendants>();
    }
    else if (identifier == UtilityTrace::IDENTIFIER)
    {
        return binaryFun<UtilityTrace>(context);
    }
    else if (identifier == BooleanNotOperator::IDENTIFIER)
    {
        return fun<BooleanNotOperator>();
    }
    else if (identifier == TypeAsOperator::IDENTIFIER)
    {
        FPExpect(context->paramList() && (context->paramList()->expression().size() == 1),
                 "wrong number of arguments for function: " + identifier);
        return std::make_any<ExpressionPtr>(
            std::make_shared<TypeAsFunction>(mRepository, nullptr, context->paramList()->expression(0)->getText()));
    }
    else if (identifier == TypesIsOperator::IDENTIFIER)
    {
        FPExpect(context->paramList() && (context->paramList()->expression().size() == 1),
                 "wrong number of arguments for function: " + identifier);
        return std::make_any<ExpressionPtr>(
            std::make_shared<TypesIsOperator>(mRepository, nullptr, context->paramList()->expression(0)->getText()));
    }
    else if (identifier == HasValue::IDENTIFIER)
    {
        return fun<HasValue>();
    }
    else if (identifier == GetValue::IDENTIFIER)
    {
        return fun<GetValue>();
    }
    else if (identifier == ConformsTo::IDENTIFIER)
    {
        return unaryFun<ConformsTo>(context);
    }
    else if (identifier == HtmlChecks::IDENTIFIER)
    {
        return fun<HtmlChecks>();
    }
    else if (identifier == ExtensionFunction::IDENTIFIER)
    {
        return unaryFun<ExtensionFunction>(context);
    }
    else if (identifier == Resolve::IDENTIFIER)
    {
        return fun<Resolve>();
    }
    FPFail("unhandled function in visitFunction: " + identifier);
    return nullptr;
}
std::any FhirPathParser::Impl::visitParamList(fhirtools::fhirpathParser::ParamListContext* context)
{
    TRACE;
    FPFail("should not be called.");
}
std::any FhirPathParser::Impl::visitQuantity(fhirtools::fhirpathParser::QuantityContext* context)
{
    TRACE;
    return std::make_any<ExpressionPtr>(std::make_shared<LiteralQuantityExpression>(mRepository, context->getText()));
}
std::any FhirPathParser::Impl::visitUnit(fhirtools::fhirpathParser::UnitContext* context)
{
    TRACE;
    FPFail("should not be called.");
}
std::any FhirPathParser::Impl::visitDateTimePrecision(fhirtools::fhirpathParser::DateTimePrecisionContext* context)
{
    TRACE;
    FPFail("should not be called.");
}
std::any
FhirPathParser::Impl::visitPluralDateTimePrecision(fhirtools::fhirpathParser::PluralDateTimePrecisionContext* context)
{
    TRACE;
    FPFail("should not be called.");
}
std::any FhirPathParser::Impl::visitTypeSpecifier(fhirtools::fhirpathParser::TypeSpecifierContext* context)
{
    TRACE;
    return visit(context->qualifiedIdentifier());
}
std::any FhirPathParser::Impl::visitQualifiedIdentifier(fhirtools::fhirpathParser::QualifiedIdentifierContext* context)
{
    TRACE;
    std::string ret;
    std::string sep;
    for (const auto& identifier : context->identifier())
    {
        ret += sep + std::any_cast<std::string>(visit(identifier));
        sep = ".";
    }
    return ret;
}
std::any FhirPathParser::Impl::visitIdentifier(fhirtools::fhirpathParser::IdentifierContext* context)
{
    TRACE;
    if (context->DELIMITEDIDENTIFIER())
    {
        return boost::trim_copy_if(context->DELIMITEDIDENTIFIER()->getText(), [](const auto& c) {
            return c == '`';
        });
    }
    return context->getText();
}

#undef TRACE

fhirtools::ExpressionPtr FhirPathParser::parse(const FhirStructureRepository* repository, std::string_view fhirPath)
{
    antlr4::ANTLRInputStream input{fhirPath};
    fhirtools::fhirpathLexer lexer{&input};
    antlr4::CommonTokenStream tokens{&lexer};
    tokens.fill();
    fhirtools::fhirpathParser parser{&tokens};
    parser.setBuildParseTree(true);
    auto* expr = parser.expression();
    if (! expr)
    {
        LOG(ERROR) << "could not get expression" << std::endl;
        return {};
    }
    Impl impl(repository);
    return std::any_cast<ExpressionPtr>(impl.visit(expr));
}

}

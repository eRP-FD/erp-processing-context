/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "FhirConstraint.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"

#include <magic_enum.hpp>

using fhirtools::FhirConstraint;

const FhirConstraint::Key& FhirConstraint::getKey() const
{
    return mKey;
}
fhirtools::Severity FhirConstraint::getSeverity() const
{
    return mSeverity;
}
const std::string& FhirConstraint::getHuman() const
{
    return mHuman;
}
const std::string& FhirConstraint::getExpression() const
{
    return mExpression;
}

std::shared_ptr<const fhirtools::Expression> FhirConstraint::parsedExpression(const FhirStructureRepository& repo,
                                                                   ExpressionCache* cache) const
{
    if (! mParsedExpression)
    {
        if (cache)
        {
            auto cacheEntry = cache->try_emplace(mExpression, std::shared_ptr<const Expression>{});
            if (cacheEntry.second)
            {
                cacheEntry.first->second = FhirPathParser::parse(&repo, getExpression());
            }
            mParsedExpression = cacheEntry.first->second;
        }
        else
        {
            mParsedExpression = FhirPathParser::parse(&repo, getExpression());
        }
    }
    return mParsedExpression;
}

decltype(auto) FhirConstraint::tie() const
{
    return std::tie(mKey, mExpression, mSeverity, mHuman);
}

std::strong_ordering FhirConstraint::operator<=>(const FhirConstraint& other) const
{
    return tie() <=> other.tie();
}

bool FhirConstraint::operator==(const FhirConstraint& other) const
{
    return tie() == other.tie();
}


FhirConstraint::Builder::Builder()
    : mConstraint(std::make_unique<FhirConstraint>())
{
}

FhirConstraint::Builder& FhirConstraint::Builder::key(FhirConstraint::Key key)
{
    mConstraint->mKey = std::move(key);
    return *this;
}
FhirConstraint::Builder& FhirConstraint::Builder::severity(const std::string& severity)
{
    auto sev = magic_enum::enum_cast<Severity>(severity);
    Expect(sev.has_value(), "invalid severity in constraint: " + severity);
    mConstraint->mSeverity = *sev;
    return *this;
}
FhirConstraint::Builder& FhirConstraint::Builder::human(std::string human)
{
    mConstraint->mHuman = std::move(human);
    return *this;
}
FhirConstraint::Builder& FhirConstraint::Builder::expression(std::string expression)
{
    mConstraint->mExpression = std::move(expression);
    return *this;
}

FhirConstraint FhirConstraint::Builder::getAndReset()
{
    FhirConstraint prev{std::move(*mConstraint)};
    mConstraint = std::make_unique<FhirConstraint>();
    return prev;
}

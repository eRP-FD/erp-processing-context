/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_TOOLS_FHIRCONSTRAINT_HXX
#define FHIR_TOOLS_FHIRCONSTRAINT_HXX

#include "fhirtools/validator/Severity.hxx"

#include <string>
#include <memory>
#include <unordered_map>

namespace fhirtools {
class Expression;
class FhirStructureRepository;

/**
 * @brief Represents a single Constraint as provided by ElementDefinition.constraint
 * @see https://simplifier.net/packages/hl7.fhir.r4.core/4.0.1/files/81633
 */
class FhirConstraint
{
public:
    using Key = std::string;
    using ExpressionCache = std::unordered_map<std::string, std::shared_ptr<const Expression>>;
    class Builder;

    [[nodiscard]] const Key& getKey() const;
    [[nodiscard]] Severity getSeverity() const;
    [[nodiscard]] const std::string& getHuman() const;
    [[nodiscard]] const std::string& getExpression() const;
    [[nodiscard]] std::shared_ptr<const Expression> parsedExpression(const FhirStructureRepository& repo,
                                                              ExpressionCache* cache = nullptr) const;


    std::strong_ordering operator <=> (const FhirConstraint&) const;
    bool operator == (const FhirConstraint&) const;

private:
    decltype(auto) tie() const;

    Key mKey;
    Severity mSeverity{};
    std::string mHuman;
    std::string mExpression;
    mutable std::shared_ptr<const Expression> mParsedExpression;
};

class FhirConstraint::Builder
{
public:
    Builder();
    Builder& key(Key key);
    Builder& severity(const std::string& severity);
    Builder& human(std::string human);
    Builder& expression(std::string expression);

    FhirConstraint getAndReset();

private:
    std::unique_ptr<FhirConstraint> mConstraint;
};

}

#endif//FHIR_TOOLS_FHIRCONSTRAINT_HXX

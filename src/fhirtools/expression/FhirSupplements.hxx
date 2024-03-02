/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */
// Contains implementation for FHIR-Resource specific FHIRPath supplements specified here:
// https://hl7.org/fhir/fhirpath.html

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_FHIRSUPPLEMENTS_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_FHIRSUPPLEMENTS_HXX

#include "fhirtools/expression/Expression.hxx"

namespace fhirtools
{
// external constant '%resource'
class PercentResource : public Expression
{
public:
    using Expression::Expression;
    Collection eval(const Collection& collection) const override;
};

// external constant '%rootResource'
class PercentRootResource : public Expression
{
public:
    using Expression::Expression;
    Collection eval(const Collection& collection) const override;
};

// https://hl7.org/fhir/fhirpath.html#functions
class ExtensionFunction : public UnaryExpression
{
public:
    static constexpr auto IDENTIFIER = "extension";
    explicit ExtensionFunction(const FhirStructureRepository* fhirStructureRepository, ExpressionPtr arg);
    Collection eval(const Collection& collection) const override;
};

// https://hl7.org/fhir/fhirpath.html#functions
class HasValue : public Expression
{
public:
    static constexpr auto IDENTIFIER = "hasValue";
    using Expression::Expression;
    Collection eval(const Collection& collection) const override;
};

// https://hl7.org/fhir/fhirpath.html#functions
class GetValue : public Expression
{
public:
    static constexpr auto IDENTIFIER = "getValue";
    using Expression::Expression;
    Collection eval(const Collection& collection) const override;
};

// https://hl7.org/fhir/fhirpath.html#functions
class Resolve : public Expression
{
public:
    static constexpr auto IDENTIFIER = "resolve";
    using Expression::Expression;
    Collection eval(const Collection& collection) const override;
};

// https://hl7.org/fhir/fhirpath.html#functions
class ElementDefintion : public Expression
{
};

// https://hl7.org/fhir/fhirpath.html#functions
class Slice : public BinaryExpression
{
};

// https://hl7.org/fhir/fhirpath.html#functions
class CheckModifiers : public Expression
{
};

// https://hl7.org/fhir/fhirpath.html#functions
class ConformsTo : public UnaryExpression
{
public:
    static constexpr auto IDENTIFIER = "conformsTo";
    explicit ConformsTo(const FhirStructureRepository* fhirStructureRepository, ExpressionPtr arg);
    Collection eval(const Collection& collection) const override;
};

// https://hl7.org/fhir/fhirpath.html#functions
class MemberOf : public UnaryExpression
{
};

// https://hl7.org/fhir/fhirpath.html#functions
class Subsumes : public UnaryExpression
{
};

// https://hl7.org/fhir/fhirpath.html#functions
class SubsumedBy : public UnaryExpression
{
};

// https://hl7.org/fhir/fhirpath.html#functions
class HtmlChecks : public Expression
{
public:
    static constexpr auto IDENTIFIER = "htmlChecks";
    using Expression::Expression;
    Collection eval(const Collection& collection) const override;
};
}


#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_FHIRSUPPLEMENTS_HXX

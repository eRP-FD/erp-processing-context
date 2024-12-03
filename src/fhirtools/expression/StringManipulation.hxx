/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_STRINGMANIPULATION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_STRINGMANIPULATION_HXX

#include "fhirtools/expression/Expression.hxx"

namespace fhirtools
{
// http://hl7.org/fhirpath/N1/#indexofsubstring-string-integer
class StringManipIndexOf : public UnaryExpression
{
public:
    static constexpr auto IDENTIFIER = "indexOf";
    using UnaryExpression::UnaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#substringstart-integer-length-integer-string
class StringManipSubstring : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "substring";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#startswithprefix-string-boolean
class StringManipStartsWith : public UnaryExpression
{
public:
    static constexpr auto IDENTIFIER = "startsWith";
    using UnaryExpression::UnaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#endswithsuffix-string-boolean
class StringManipEndsWith : public UnaryExpression
{
};

// http://hl7.org/fhirpath/N1/#containssubstring-string-boolean
class StringManipContains : public UnaryExpression
{
public:
    static constexpr auto IDENTIFIER = "contains";
    using UnaryExpression::UnaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#upper-string
class StringManipUpper : public Expression
{
};

// http://hl7.org/fhirpath/N1/#lower-string
class StringManipLower : public Expression
{
};

// http://hl7.org/fhirpath/N1/#replacepattern-string-substitution-string-string
class StringManipReplace : public BinaryExpression
{
};

// http://hl7.org/fhirpath/N1/#matchesregex-string-boolean
class StringManipMatches : public UnaryExpression
{
public:
    static constexpr auto IDENTIFIER = "matches";
    using UnaryExpression::UnaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#replacematchesregex-string-substitution-string-string
class StringManipReplaceMatches : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "replaceMatches";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#length-integer
class StringManipLength : public Expression
{
public:
    static constexpr auto IDENTIFIER = "length";
    using Expression::Expression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#tochars-collection
class StringManipToChars : public Expression
{
public:
    static constexpr auto IDENTIFIER = "toChars";
};

// http://hl7.org/fhirpath/N1/#addition
// Only implemented for string concatenation and integer+integer
class PlusOperator : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "+";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#string-concatenation
class AmpersandOperator : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "&";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

// https://build.fhir.org/ig/HL7/FHIRPath/#splitseparator-string--collection
// STU: Date of Approval: 2024-07-24
class StringManipSplit : public UnaryExpression
{
public:
    static constexpr auto IDENTIFIER = "split";
    using UnaryExpression::UnaryExpression;
    Collection eval(const Collection& collection) const override;
private:
    void split(Collection& result, std::string_view str, std::string_view delimiter) const;
    void chars(Collection& result, std::string_view str) const;
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_STRINGMANIPULATION_HXX

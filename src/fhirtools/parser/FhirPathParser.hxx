/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef FHIR_TOOLS_FHIR_PATH_PARSER_FHIRPATHPARSER_HXX
#define FHIR_TOOLS_FHIR_PATH_PARSER_FHIRPATHPARSER_HXX

#include "fhirtools/expression/Expression.hxx"

namespace fhirtools
{
class FhirStructureRepository;

class FhirPathParser
{
public:
    /// @brief parses and compiles one FHIRPath expression
    ///        and returns the pointer to root node in the compiled expression tree
    [[nodiscard]] static ExpressionPtr parse(const FhirStructureRepository* repository, std::string_view fhirPath);

    [[nodiscard]] static std::string unescapeStringLiteral(const std::string_view& str, char stringDelimiter);

private:
    class Impl;
};

}
#endif// FHIR_TOOLS_FHIR_PATH_PARSER_FHIRPATHPARSER_HXX

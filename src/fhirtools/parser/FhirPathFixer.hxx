/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_TOOLS_FHIR_PATH_PARSER_FHIRPATHFIXER_HXX
#define FHIR_TOOLS_FHIR_PATH_PARSER_FHIRPATHFIXER_HXX

#include <string_view>

namespace fhirtools
{
class FhirStructureRepository;

class FhirPathFixer
{
public:
    static std::string_view fixFhirPath(std::string_view fhirPath);
};

} // namespace fhirtools

#endif

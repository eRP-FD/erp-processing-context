#ifndef FHIR_PATH_CONSTANTS_HXX
#define FHIR_PATH_CONSTANTS_HXX

#include "fhirtools/util/XmlHelper.hxx"

namespace fhirtools::constants
{
static constexpr XmlStringView namespaceUri{"http://hl7.org/fhir"};
// Resources:
static constexpr XmlStringView bundleUrl{"http://hl7.org/fhir/StructureDefinition/Bundle"};
static constexpr XmlStringView compositionUrl{"http://hl7.org/fhir/StructureDefinition/Composition"};
static constexpr XmlStringView domainResourceUrl{"http://hl7.org/fhir/StructureDefinition/DomainResource"};
static constexpr XmlStringView resourceUrl{"http://hl7.org/fhir/StructureDefinition/Resource"};
// ValueSets:
static constexpr XmlStringView resourceTypesUrl{"http://hl7.org/fhir/resource-types"};

// System Types:
static constexpr std::string_view systemTypePrefix{"http://hl7.org/fhirpath/System."};

static constexpr std::string_view systemTypeBoolean{"http://hl7.org/fhirpath/System.Boolean"};
static constexpr std::string_view systemTypeString{"http://hl7.org/fhirpath/System.String"};
static constexpr std::string_view systemTypeDate{"http://hl7.org/fhirpath/System.Date"};
static constexpr std::string_view systemTypeTime{"http://hl7.org/fhirpath/System.Time"};
static constexpr std::string_view systemTypeDateTime{"http://hl7.org/fhirpath/System.DateTime"};
static constexpr std::string_view systemTypeDecimal{"http://hl7.org/fhirpath/System.Decimal"};
static constexpr std::string_view systemTypeInteger{"http://hl7.org/fhirpath/System.Integer"};

// Misc:
static constexpr std::string_view typePlaceholder{"[x]"};


}


#endif// FHIR_PATH_CONSTANTS_HXX

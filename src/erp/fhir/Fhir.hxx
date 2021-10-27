/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_FHIR_HXX
#define ERP_PROCESSING_CONTEXT_FHIR_HXX

#include "erp/xml/XmlHelper.hxx"
#include "erp/fhir/FhirConverter.hxx"
#include "erp/fhir/FhirStructureRepository.hxx"

//#include <set>
//#include <memory>
#include <string_view>

class FhirConverter;
class FhirStructureRepository;

class Fhir final
{
public:
    static constexpr XmlStringView namespaceUri {"http://hl7.org/fhir"};
    static constexpr XmlStringView resourceUrl {"http://hl7.org/fhir/StructureDefinition/Resource"};
    static constexpr std::string_view systemTypePrefix{"http://hl7.org/fhirpath/System."};

    static constexpr std::string_view systemTypeBoolean{"http://hl7.org/fhirpath/System.Boolean"};
    static constexpr std::string_view systemTypeString{"http://hl7.org/fhirpath/System.String"};
    static constexpr std::string_view systemTypeDate{"http://hl7.org/fhirpath/System.Date"};
    static constexpr std::string_view systemTypeTime{"http://hl7.org/fhirpath/System.Time"};
    static constexpr std::string_view systemTypeDateTime{"http://hl7.org/fhirpath/System.DateTime"};
    static constexpr std::string_view systemTypeDecimal{"http://hl7.org/fhirpath/System.Decimal"};
    static constexpr std::string_view systemTypeInteger{"http://hl7.org/fhirpath/System.Integer"};

    static constexpr std::string_view typePlaceholder{"[x]"};

    static const Fhir& instance();

    const FhirConverter& converter() const { return mConverter; }
    const FhirStructureRepository& structureRepository() const { return mStructureRepository; }

private:
    Fhir();

    FhirConverter mConverter;
    FhirStructureRepository mStructureRepository;
};


#endif// ERP_PROCESSING_CONTEXT_FHIR_HXX

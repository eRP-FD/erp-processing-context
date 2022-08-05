/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_FHIR_HXX
#define ERP_PROCESSING_CONTEXT_FHIR_HXX

#include "erp/fhir/FhirConverter.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/util/XmlHelper.hxx"

//#include <set>
//#include <memory>
#include <string_view>

class FhirConverter;

class Fhir final
{
public:
    static const Fhir& instance();

    const FhirConverter& converter() const { return mConverter; }
    const fhirtools::FhirStructureRepository& structureRepository() const { return mStructureRepository; }

private:
    Fhir();

    FhirConverter mConverter;
    fhirtools::FhirStructureRepository mStructureRepository;
};


#endif// ERP_PROCESSING_CONTEXT_FHIR_HXX

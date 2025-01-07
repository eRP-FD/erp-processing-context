// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
// non-exclusively licensed to gematik GmbH

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAMEDICATIONTYPEEXTENSION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAMEDICATIONTYPEEXTENSION_HXX

#include "shared/model/Extension.hxx"

namespace model
{

class EPAMedicationTypeExtension : public Extension<EPAMedicationTypeExtension>
{
public:
    EPAMedicationTypeExtension();
    static constexpr auto url =
        "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication-type-extension";
private:
    using Extension::Extension;
};

extern template class Resource<EPAMedicationTypeExtension>;
extern template class Extension<EPAMedicationTypeExtension>;

}

#endif

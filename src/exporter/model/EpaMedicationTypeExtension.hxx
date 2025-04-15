// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAMEDICATIONTYPEEXTENSION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAMEDICATIONTYPEEXTENSION_HXX

#include "shared/model/Extension.hxx"

namespace model
{

class EPAMedicationTypeExtension : public Extension<EPAMedicationTypeExtension>
{
public:
    EPAMedicationTypeExtension(std::string_view code, std::string_view display);
    static constexpr auto url =
        "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication-type-extension";
  static constexpr auto MedicinalProductPackageCode = "781405001";
  static constexpr auto MedicinalProductPackageDisplay = "Medicinal product package (product)";
  static constexpr auto ExtemporaneousPreparationCode = "1208954007";
  static constexpr auto ExtemporaneousPreparationDisplay = "Extemporaneous preparation (product)";
  static constexpr auto PharmaceuticalBiologicProductCode = "373873005";
  static constexpr auto PharmaceuticalBiologicProductDisplay = "Pharmaceutical / biologic product (product)";
private:
    using Extension::Extension;
};

extern template class Resource<EPAMedicationTypeExtension>;
extern template class Extension<EPAMedicationTypeExtension>;

}

#endif

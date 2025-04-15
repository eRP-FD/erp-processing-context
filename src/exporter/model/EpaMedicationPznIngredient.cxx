// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "exporter/model/EpaMedicationPznIngredient.hxx"
#include "exporter/model/EpaMedicationTypeExtension.hxx"
#include "shared/util/Uuid.hxx"

#include "shared/model/Pzn.hxx"

namespace model
{

EPAMedicationPZNIngredient::EPAMedicationPZNIngredient(const Pzn& pzn, std::string_view display)
    : MedicationBase(profileType)
{
    setResourceType("Medication");
    using namespace resource::elements;
    setId(Uuid{}.toString());
    EPAMedicationTypeExtension typeExtension{EPAMedicationTypeExtension::MedicinalProductPackageCode,
                                             EPAMedicationTypeExtension::MedicinalProductPackageDisplay};
    setValue(rapidjson::Pointer{resource::ElementName::path(extension, 0)}, typeExtension.jsonDocument());
    setValue(rapidjson::Pointer{resource::ElementName::path(code, coding, 0, system)}, resource::code_system::pzn);
    setValue(rapidjson::Pointer{resource::ElementName::path(code, coding, 0, code)}, pzn.id());
    setValue(rapidjson::Pointer{resource::ElementName::path(code, coding, 0, resource::elements::display)}, display);
    setValue(rapidjson::Pointer{resource::ElementName::path(code, text)}, display);
}

template class MedicationBase<EPAMedicationPZNIngredient>;
}

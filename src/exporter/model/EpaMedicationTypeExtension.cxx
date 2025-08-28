// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "exporter/model/EpaMedicationTypeExtension.hxx"
namespace model
{

namespace
{
const char* mapCode(EPAMedicationTypeExtension::EPAMedicationTypeExtension::Type type)
{
    switch (type)
    {
        case EPAMedicationTypeExtension::Type::MedicinalProductPackage:
            return EPAMedicationTypeExtension::MedicinalProductPackageCode;
        case EPAMedicationTypeExtension::Type::ExtemporaneousPreparation:
            return EPAMedicationTypeExtension::ExtemporaneousPreparationCode;
        case EPAMedicationTypeExtension::Type::PharmaceuticalBiologicProduct:
            return EPAMedicationTypeExtension::PharmaceuticalBiologicProductCode;
    }
    ModelFail("invalid type");
}
const char* mapDisplay(EPAMedicationTypeExtension::EPAMedicationTypeExtension::Type type)
{
    switch (type)
    {
        case EPAMedicationTypeExtension::Type::MedicinalProductPackage:
            return EPAMedicationTypeExtension::MedicinalProductPackageDisplay;
        case EPAMedicationTypeExtension::Type::ExtemporaneousPreparation:
            return EPAMedicationTypeExtension::ExtemporaneousPreparationDisplay;
        case EPAMedicationTypeExtension::Type::PharmaceuticalBiologicProduct:
            return EPAMedicationTypeExtension::PharmaceuticalBiologicProductDisplay;
    }
    ModelFail("invalid type");
}
}

EPAMedicationTypeExtension::EPAMedicationTypeExtension(Type type)
    : EPAMedicationTypeExtension(mapCode(type), mapDisplay(type))
{
}

EPAMedicationTypeExtension::EPAMedicationTypeExtension(std::string_view code, std::string_view display)
{
    using namespace resource::elements;
    setValue(rapidjson::Pointer{resource::ElementName::path(resource::elements::url)}, url);
    setValue(rapidjson::Pointer{resource::ElementName::path(valueCoding, system)}, resource::code_system::sct);
    setValue(rapidjson::Pointer{resource::ElementName::path(valueCoding, resource::elements::code)}, code);
    setValue(rapidjson::Pointer{resource::ElementName::path(valueCoding, resource::elements::display)}, display);
}

template class Extension<EPAMedicationTypeExtension>;
}

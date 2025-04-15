// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "exporter/model/EpaMedicationTypeExtension.hxx"
namespace model
{
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

// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
// non-exclusively licensed to gematik GmbH

#include "exporter/model/EpaMedicationTypeExtension.hxx"
namespace model
{
EPAMedicationTypeExtension::EPAMedicationTypeExtension()
{
    using namespace resource::elements;
    setValue(rapidjson::Pointer{resource::ElementName::path(resource::elements::url)}, url);
    setValue(rapidjson::Pointer{resource::ElementName::path(valueCoding, system)}, resource::code_system::sct);
    setValue(rapidjson::Pointer{resource::ElementName::path(valueCoding, code)}, "781405001");
    setValue(rapidjson::Pointer{resource::ElementName::path(valueCoding, display)},
             "Medicinal product package (product)");
}

template class Extension<EPAMedicationTypeExtension>;
}

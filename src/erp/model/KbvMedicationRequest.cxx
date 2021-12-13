/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/extensions/KBVMultiplePrescription.hxx"

namespace model
{

KbvMedicationRequest::KbvMedicationRequest(NumberAsStringParserDocument&& document)
    : Resource<KbvMedicationRequest, ResourceVersion::KbvItaErp>(std::move(document))
{
}


std::optional<std::string_view> KbvMedicationRequest::statusCoPaymentExtension() const
{
    static const rapidjson::Pointer extensionArrayPointer("/extension");
    static const rapidjson::Pointer urlPointer("/url");
    static const rapidjson::Pointer codePointer("/valueCoding/code");
    return findStringInArray(extensionArrayPointer, urlPointer, resource::structure_definition::kbvExErpStatusCoPayment,
                             codePointer);
}

bool model::KbvMedicationRequest::isMultiplePrescription() const
{
    A_22068.start("Bundle.[entry.]MedicationRequest.extension:Mehrfachverordnung");
    auto multiplePrescription = getExtension<KBVMultiplePrescription>();
    bool res = multiplePrescription.has_value() && multiplePrescription->isMultiplePrescription();
    A_22068.finish();
    return res;
}

std::optional<Dosage> KbvMedicationRequest::dosageInstruction() const
{
    static const rapidjson::Pointer dosageInstructionPointer(resource::ElementName::path("dosageInstruction", "0"));
    const auto* entry = getValue(dosageInstructionPointer);
    if (entry)
    {
        return Dosage::fromJson(*entry);
    }
    return std::nullopt;
}

Dosage::Dosage(NumberAsStringParserDocument&& document)
    : Resource<Dosage, ResourceVersion::KbvItaErp>(std::move(document))
{
}

std::optional<std::string_view> Dosage::text() const
{
    static const rapidjson::Pointer textPointer(resource::ElementName::path(resource::elements::text));
    return getOptionalStringValue(textPointer);
}


}

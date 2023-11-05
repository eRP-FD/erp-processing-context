/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/ErpConstants.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/extensions/KBVMultiplePrescription.hxx"

#include <date/tz.h>

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
    auto multiplePrescription = getExtension<KBVMultiplePrescription>();
    bool res = multiplePrescription.has_value() && multiplePrescription->isMultiplePrescription();
    return res;
}

std::optional<date::year_month_day> KbvMedicationRequest::mvoEndDate() const
{
    auto mPExt = getExtension<model::KBVMultiplePrescription>();
    if (mPExt)
    {
        if (mPExt->isMultiplePrescription())
        {
            auto endDate = mPExt->endDate();
            if (endDate.has_value())
            {
                return date::year_month_day{endDate->localDay(Timestamp::GermanTimezone)};
            }
        }
    }
    return std::nullopt;
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

model::Timestamp KbvMedicationRequest::authoredOn() const
{
    static const rapidjson::Pointer authoredOnPointer(resource::ElementName::path("authoredOn"));
    const auto authoredOnStr = getStringValue(authoredOnPointer);
    return model::Timestamp::fromGermanDate(std::string{authoredOnStr});
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

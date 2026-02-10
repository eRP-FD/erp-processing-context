// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "shared/model/KbvMedicationBase.hxx"

namespace model
{

KbvMedicationGeneric::KbvMedicationGeneric(NumberAsStringParserDocument&& document)
    : MedicationBase<KbvMedicationGeneric>(std::move(document))
{
}

bool isArzneimittelverordnung(MedicationCategoryCode category)
{
    switch (category)
    {
        case MedicationCategoryCode::ARZNEIMITTELVERORDNUNG:
            return true;
        case MedicationCategoryCode::BTM:
        case MedicationCategoryCode::TREZEPT:
            break;
    }
    return false;
}

bool isBTM(MedicationCategoryCode category)
{
    switch (category)
    {
        case MedicationCategoryCode::BTM:
            return true;
        case MedicationCategoryCode::ARZNEIMITTELVERORDNUNG:
        case MedicationCategoryCode::TREZEPT:
            break;
    }
    return false;
}

bool isTRezept(MedicationCategoryCode category)
{
    switch (category)
    {
        case MedicationCategoryCode::TREZEPT:
            return true;
        case MedicationCategoryCode::ARZNEIMITTELVERORDNUNG:
        case MedicationCategoryCode::BTM:
            break;
    }
    return false;
}

MedicationCategoryCode KbvMedicationGeneric::getCategoryCode() const
{
    const auto categoryExtension = this->template getExtension<KBVMedicationCategory>();
    ModelExpect(categoryExtension.has_value(), "Missing medication category extension.");
    ModelExpect(categoryExtension->valueCodingCode().has_value(), "Missing medication category code.");
    if (*categoryExtension->valueCodingCode() == "00")
    {
        return MedicationCategoryCode::ARZNEIMITTELVERORDNUNG;
    }
    if (*categoryExtension->valueCodingCode() == "01")
    {
        return MedicationCategoryCode::BTM;
    }
    if (*categoryExtension->valueCodingCode() == "02")
    {
        return MedicationCategoryCode::TREZEPT;
    }
    ModelFail("Unknown medication category code " + std::string{*categoryExtension->valueCodingCode()});
}

}
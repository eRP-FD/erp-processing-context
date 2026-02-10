/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONBASE_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONBASE_HXX

#include "shared/model/MedicationBase.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/extensions/KBVMedicationCategory.hxx"

#include <optional>

class ErpElement;
class XmlValidator;

namespace model
{
enum class MedicationCategoryCode
{
    ARZNEIMITTELVERORDNUNG,
    BTM,
    TREZEPT
};
[[nodiscard]] bool isArzneimittelverordnung(MedicationCategoryCode category);
[[nodiscard]] bool isBTM(MedicationCategoryCode category);
[[nodiscard]] bool isTRezept(MedicationCategoryCode category);

class KbvMedicationGeneric : public MedicationBase<KbvMedicationGeneric>
{
public:
    KbvMedicationGeneric(NumberAsStringParserDocument&& document);

    [[nodiscard]] MedicationCategoryCode getCategoryCode() const;

private:
    friend MedicationBase<KbvMedicationGeneric>;
};


// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<KbvMedicationGeneric>;
}

#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONBASE_HXX

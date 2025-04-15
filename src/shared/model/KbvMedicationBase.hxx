/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONBASE_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONBASE_HXX

#include "shared/model/Resource.hxx"
#include "shared/model/MedicationBase.hxx"
#include "shared/model/extensions/KBVMedicationCategory.hxx"

#include <optional>

class ErpElement;
class XmlValidator;

namespace model
{

class KbvMedicationGeneric : public MedicationBase<KbvMedicationGeneric>
{
public:
    KbvMedicationGeneric(NumberAsStringParserDocument&& document)
        : MedicationBase<KbvMedicationGeneric>(std::move(document))
    {
    }

    bool isNarcotics() const
    {
        const auto narcotics = this->template getExtension<KBVMedicationCategory>();
        ModelExpect(narcotics.has_value(), "Missing medication category.");
        ModelExpect(narcotics->valueCodingCode().has_value(), "Missing medication category code.");
        return narcotics->valueCodingCode().value() != "00";
    }

private:
    friend MedicationBase<KbvMedicationGeneric>;
};


// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<KbvMedicationGeneric>;
}

#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONBASE_HXX

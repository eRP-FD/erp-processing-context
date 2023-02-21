/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONBASE_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONBASE_HXX

#include "erp/model/Resource.hxx"
#include "erp/model/extensions/KBVMedicationCategory.hxx"

namespace model
{

template<class TDerivedMedication, typename SchemaVersionType>
// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvMedicationBase : public Resource<TDerivedMedication, SchemaVersionType>
{
public:
    explicit KbvMedicationBase(NumberAsStringParserDocument&& document);
    static constexpr auto resourceTypeName = "Medication";
    bool isNarcotics() const
    {
        const auto narcotics = this->template getExtension<KBVMedicationCategory>();
        ModelExpect(narcotics.has_value(), "Missing medication category.");
        ModelExpect(narcotics->valueCodingCode().has_value(), "Missing medication category code.");
        return narcotics->valueCodingCode().value() != "00";
    }

private:
    friend Resource<KbvMedicationBase, ResourceVersion::KbvItaErp>;
};

template<class TDerivedMedication, typename SchemaVersionType>
KbvMedicationBase<TDerivedMedication, SchemaVersionType>::KbvMedicationBase(NumberAsStringParserDocument&& document)
    : Resource<TDerivedMedication, SchemaVersionType>(std::move(document))
{
}

class KbvMedicationGeneric : public KbvMedicationBase<KbvMedicationGeneric, ResourceVersion::KbvItaErp>
{
public:
    KbvMedicationGeneric(NumberAsStringParserDocument&& document)
        : KbvMedicationBase<KbvMedicationGeneric, ResourceVersion::KbvItaErp>(std::move(document))
    {
    }


private:
    friend KbvMedicationBase<KbvMedicationGeneric, ResourceVersion::KbvItaErp>;
};

}

#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONBASE_HXX

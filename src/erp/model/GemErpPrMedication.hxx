/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef ERP_PROCESSING_CONTEXT_GEM_ERP_PR_MEDICATION_HXX
#define ERP_PROCESSING_CONTEXT_GEM_ERP_PR_MEDICATION_HXX

#include "shared/model/MedicationBase.hxx"


namespace model
{
class GemErpPrMedication : public MedicationBase<GemErpPrMedication>
{
public:
    static constexpr auto profileType = ProfileType::GEM_ERP_PR_Medication;

    using MedicationBase::MedicationBase;


private:
    friend class Resource<GemErpPrMedication>;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<GemErpPrMedication>;

}

#endif// ERP_PROCESSING_CONTEXT_GEM_ERP_PR_MEDICATION_HXX

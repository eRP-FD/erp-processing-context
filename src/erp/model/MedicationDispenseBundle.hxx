/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEBUNDLE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEBUNDLE_HXX

#include "erp/model/Bundle.hxx"
#include "erp/model/ProfileType.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class MedicationDispenseBundle : public BundleBase<MedicationDispenseBundle>
{
public:
    static constexpr auto profileType = ProfileType::MedicationDispenseBundle;

    using BundleBase<MedicationDispenseBundle>::BundleBase;
    using Resource<MedicationDispenseBundle>::fromXml;
    using Resource<MedicationDispenseBundle>::fromJson;

    void prepare() override;
    std::optional<model::Timestamp> getValidationReferenceTimestamp() const override;
};


// NOLINTBEGIN(bugprone-exception-escape)
extern template class BundleBase<MedicationDispenseBundle>;
extern template class Resource<MedicationDispenseBundle>;
// NOLINTEND(bugprone-exception-escape)
}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEBUNDLE_HXX

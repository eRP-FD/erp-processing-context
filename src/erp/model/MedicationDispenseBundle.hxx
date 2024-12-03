/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEBUNDLE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEBUNDLE_HXX

#include "shared/model/Bundle.hxx"
#include "shared/model/ProfileType.hxx"

#include <vector>

namespace model
{
class GemErpPrMedication;
class MedicationDispense;

// NOLINTNEXTLINE(bugprone-exception-escape)
class MedicationDispenseBundle : public BundleBase<MedicationDispenseBundle>
{
public:
    static constexpr auto profileType = ProfileType::MedicationDispenseBundle;

    explicit MedicationDispenseBundle(const std::string& linkBase,
                                      const std::vector<model::MedicationDispense>& medicationDisplenses,
                                      const std::vector<model::GemErpPrMedication>& medications);

    using BundleBase<MedicationDispenseBundle>::BundleBase;
    using Resource<MedicationDispenseBundle>::fromXml;
    using Resource<MedicationDispenseBundle>::fromJson;

    void prepare() override;
    Timestamp whenHandedOver() const;
    std::optional<model::Timestamp> getValidationReferenceTimestamp() const override;
};


// NOLINTBEGIN(bugprone-exception-escape)
extern template class BundleBase<MedicationDispenseBundle>;
extern template class Resource<MedicationDispenseBundle>;
// NOLINTEND(bugprone-exception-escape)
}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEBUNDLE_HXX

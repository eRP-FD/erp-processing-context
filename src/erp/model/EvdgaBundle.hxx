/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_EVGDABUNDLE_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_EVGDABUNDLE_HXX


#include "shared/model/Bundle.hxx"

namespace model {

class EvdgaBundle : public BundleBase<EvdgaBundle> {
public:

    static constexpr ProfileType profileType = ProfileType::KBV_PR_EVDGA_Bundle;

    EvdgaBundle(EvdgaBundle&&) = default;
    ~EvdgaBundle() override = default;

    using BundleBase::BundleBase;
    using Resource::fromJson;
    using Resource::fromJsonNoValidation;
    using Resource::fromXml;

    [[nodiscard]] std::optional<model::Timestamp> getValidationReferenceTimestamp() const override;
    void additionalValidation() const override;

    // authoredOn from DeviceRequest entry
    [[nodiscard]] Timestamp authoredOn() const;
};

extern template class BundleBase<EvdgaBundle>;
extern template class Resource<EvdgaBundle>;



}
#endif// ERP_PROCESSING_CONTEXT_MODEL_EVGDABUNDLE_HXX

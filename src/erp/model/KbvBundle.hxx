/*
* (C) Copyright IBM Deutschland GmbH 2021, 2024
* (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
*/

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVBUNDLE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVBUNDLE_HXX

#include "shared/model/Bundle.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvBundle : public BundleBase<KbvBundle>
{
public:
    static constexpr ProfileType profileType = ProfileType::KBV_PR_ERP_Bundle;

    using BundleBase::BundleBase;
    using Resource::fromJson;
    using Resource::fromJsonNoValidation;
    using Resource::fromXml;

    void additionalValidation() const override;

    std::optional<model::Timestamp> getValidationReferenceTimestamp() const override;
};

extern template class BundleBase<KbvBundle>;
extern template class Resource<KbvBundle>;
}


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVBUNDLE_HXX

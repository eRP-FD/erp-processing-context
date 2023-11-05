/*
* (C) Copyright IBM Deutschland GmbH 2021, 2023
* (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
*/

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVBUNDLE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVBUNDLE_HXX

#include "erp/model/Bundle.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvBundle : public BundleBase<KbvBundle, ResourceVersion::KbvItaErp>
{
public:
    using BundleBase<KbvBundle, ResourceVersion::KbvItaErp>::BundleBase;
    using Resource<KbvBundle, ResourceVersion::KbvItaErp>::fromXml;
    using Resource<KbvBundle, ResourceVersion::KbvItaErp>::fromJson;
    using Resource<KbvBundle, ResourceVersion::KbvItaErp>::fromJsonNoValidation;

    void additionalValidation() const override;

    std::optional<model::Timestamp> getValidationReferenceTimestamp() const override;
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVBUNDLE_HXX

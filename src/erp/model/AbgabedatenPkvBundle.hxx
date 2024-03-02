/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_ABGABEDATENPKVBUNDLE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_ABGABEDATENPKVBUNDLE_HXX

#include "erp/model/Bundle.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class AbgabedatenPkvBundle final : public BundleBase<AbgabedatenPkvBundle, ResourceVersion::AbgabedatenPkv>
{
public:
    using BundleBase<AbgabedatenPkvBundle, ResourceVersion::AbgabedatenPkv>::BundleBase;
    using Resource<AbgabedatenPkvBundle, ResourceVersion::AbgabedatenPkv>::fromXml;
    using Resource<AbgabedatenPkvBundle, ResourceVersion::AbgabedatenPkv>::fromJson;

    std::optional<model::Timestamp> getValidationReferenceTimestamp() const override;

    static constexpr auto resourceTypeName = "Bundle";
};

} // namespace model

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_ABGABEDATENPKVBUNDLE_HXX

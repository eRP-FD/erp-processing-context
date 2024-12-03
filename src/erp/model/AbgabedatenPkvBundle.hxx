/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_ABGABEDATENPKVBUNDLE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_ABGABEDATENPKVBUNDLE_HXX

#include "shared/model/Bundle.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class AbgabedatenPkvBundle final : public BundleBase<AbgabedatenPkvBundle>
{
public:
    static constexpr auto resourceTypeName = "Bundle";
    static constexpr auto profileType = ProfileType::DAV_DispenseItem;

    using BundleBase<AbgabedatenPkvBundle>::BundleBase;
    using Resource<AbgabedatenPkvBundle>::fromXml;
    using Resource<AbgabedatenPkvBundle>::fromJson;

    std::optional<model::Timestamp> getValidationReferenceTimestamp() const override;
};

extern template class BundleBase<AbgabedatenPkvBundle>;
extern template class Resource<AbgabedatenPkvBundle>;

} // namespace model

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_ABGABEDATENPKVBUNDLE_HXX

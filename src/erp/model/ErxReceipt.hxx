/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_ERXRECEIPT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_ERXRECEIPT_HXX

#include "erp/model/Binary.hxx"
#include "erp/model/Composition.hxx"
#include "erp/model/Device.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/util/Uuid.hxx"

#include <rapidjson/document.h>

namespace model
{

// Reduced version of ErxReceipt resource, contains only functionality currently needed;

// NOLINTNEXTLINE(bugprone-exception-escape)
class ErxReceipt : public BundleBase<ErxReceipt>
{
public:
    static constexpr auto profileType = ProfileType::Gem_erxReceiptBundle;

    ErxReceipt(const Uuid& bundleId, const std::string& selfLink, const model::PrescriptionId& prescriptionId,
               const model::Composition& composition, const std::string& deviceIdentifier, const model::Device& device,
               const std::string& prescriptionDigestIdentifier, const ::model::Binary& prescriptionDigest);

    using BundleBase<ErxReceipt>::BundleBase;
    using Resource<ErxReceipt>::fromXml;
    using Resource<ErxReceipt>::fromJson;
    using Resource<ErxReceipt>::fromJsonNoValidation;

    model::PrescriptionId prescriptionId() const;
    model::Composition composition() const;
    model::Device device() const;
    ::model::Binary prescriptionDigest() const;

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class BundleBase<ErxReceipt>;
extern template class Resource<ErxReceipt>;
// NOLINTEND(bugprone-exception-escape)

} // namespace model

#endif

/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_ERXRECEIPT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_ERXRECEIPT_HXX

#include "erp/model/Binary.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/Composition.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/util/Uuid.hxx"

#include <rapidjson/document.h>

namespace model
{

// Reduced version of ErxReceipt resource, contains only functionality currently needed;

// NOLINTNEXTLINE(bugprone-exception-escape)
class ErxReceipt : public BundleBase<ErxReceipt>
{
public:
    ErxReceipt(const Uuid& bundleId, const std::string& selfLink, const model::PrescriptionId& prescriptionId,
               const model::Composition& composition, const std::string& deviceIdentifier, const model::Device& device,
               const std::string& prescriptionDigestIdentifier, const ::model::Binary& prescriptionDigest,
               model::ResourceVersion::DeGematikErezeptWorkflowR4 profileVersion =
                   ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>());

    using BundleBase<ErxReceipt>::BundleBase;
    using Resource<ErxReceipt>::fromXml;
    using Resource<ErxReceipt>::fromJson;
    using Resource<ErxReceipt>::fromJsonNoValidation;

    model::PrescriptionId prescriptionId() const;
    model::Composition composition() const;
    model::Device device() const;
    ::model::Binary prescriptionDigest() const;
};

} // namespace model

#endif

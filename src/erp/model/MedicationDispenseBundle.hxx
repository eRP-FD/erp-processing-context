/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEBUNDLE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEBUNDLE_HXX

#include "erp/model/Bundle.hxx"
#include "erp/validation/SchemaType.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class MedicationDispenseBundle : public BundleBase<MedicationDispenseBundle>
{
public:
    static constexpr SchemaType schemaType = SchemaType::MedicationDispenseBundle;

    using BundleBase<MedicationDispenseBundle>::BundleBase;
    using Resource<MedicationDispenseBundle>::fromXml;
    using Resource<MedicationDispenseBundle>::fromJson;

    std::optional<model::Timestamp> getValidationReferenceTimestamp() const override;
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEBUNDLE_HXX

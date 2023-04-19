/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
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
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEBUNDLE_HXX

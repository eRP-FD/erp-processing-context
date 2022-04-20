/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEBUNDLE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEBUNDLE_HXX

#include "erp/model/Bundle.hxx"

namespace model
{

class MedicationDispenseBundle : public BundleBase<MedicationDispenseBundle, ResourceVersion::NotProfiled>
{
public:
    using BundleBase<MedicationDispenseBundle, ResourceVersion::NotProfiled>::BundleBase;
    using Resource<MedicationDispenseBundle, ResourceVersion::NotProfiled>::fromXml;
    using Resource<MedicationDispenseBundle, ResourceVersion::NotProfiled>::fromJson;
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_MEDICATIONDISPENSEBUNDLE_HXX

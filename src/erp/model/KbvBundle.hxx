/*
* (C) Copyright IBM Deutschland GmbH 2021
* (C) Copyright IBM Corp. 2021
*/

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVBUNDLE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVBUNDLE_HXX

#include "erp/model/Bundle.hxx"

namespace model
{

class KbvBundle : public BundleBase<KbvBundle, ResourceVersion::KbvItaErp>
{
public:
    using BundleBase<KbvBundle, ResourceVersion::KbvItaErp>::BundleBase;
    using Resource<KbvBundle, ResourceVersion::KbvItaErp>::fromXml;
    using Resource<KbvBundle, ResourceVersion::KbvItaErp>::fromJson;
    using Resource<KbvBundle, ResourceVersion::KbvItaErp>::fromJsonNoValidation;

    void additionalValidation() const override;
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVBUNDLE_HXX

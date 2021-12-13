/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONMODELHELPER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONMODELHELPER_HXX

#include "erp/model/Resource.hxx"

namespace model
{

class KbvMedicationModelHelper : public model::Resource<KbvMedicationModelHelper, model::ResourceVersion::KbvItaErp>
{
public:
    static constexpr auto resourceTypeName = "Medication";

    [[nodiscard]] SchemaType getProfile() const;

private:
    friend model::Resource<KbvMedicationModelHelper, model::ResourceVersion::KbvItaErp>;
    explicit KbvMedicationModelHelper(model::NumberAsStringParserDocument&& document);
};

}


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_KBVMEDICATIONMODELHELPER_HXX

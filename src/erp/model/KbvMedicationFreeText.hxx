/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONFREETEXT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONFREETEXT_HXX

#include "erp/model/Resource.hxx"

namespace model
{

class KbvMedicationFreeText : public Resource<KbvMedicationFreeText, ResourceVersion::KbvItaErp>
{
public:
private:
    friend Resource<KbvMedicationFreeText, ResourceVersion::KbvItaErp>;
    explicit KbvMedicationFreeText(NumberAsStringParserDocument&& document);
};

}


#endif//ERP_PROCESSING_CONTEXT_MODEL_KBVMEDICATIONFREETEXT_HXX

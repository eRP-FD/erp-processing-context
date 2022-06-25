/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVMEDICATIONCATEGORY_H
#define ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVMEDICATIONCATEGORY_H

#include "erp/model/Extension.hxx"

namespace model
{

class KBVMedicationCategory : public model::Extension
{
public:
    using Extension::Extension;
    static constexpr auto url = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Category";

    friend std::optional<KBVMedicationCategory> ResourceBase::getExtension<KBVMedicationCategory>(const std::string_view&) const;
};

}

#endif// ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVMEDICATIONCATEGORY_H

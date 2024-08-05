/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVMEDICATIONCATEGORY_H
#define ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVMEDICATIONCATEGORY_H

#include "erp/model/Extension.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KBVMedicationCategory : public Extension<KBVMedicationCategory>
{
public:
    using Extension::Extension;
    static constexpr auto url = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Medication_Category";
};

extern template class Extension<KBVMedicationCategory>;
extern template class Resource<KBVMedicationCategory>;
}

#endif// ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVMEDICATIONCATEGORY_H

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVEXERPACCIDENT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVEXERPACCIDENT_HXX

#include "erp/model/Extension.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KBVEXERPAccident : public Extension<KBVEXERPAccident>
{
public:
    using Extension::Extension;
    static constexpr auto url = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Accident";
};

extern template class Extension<KBVEXERPAccident>;
extern template class Resource<KBVEXERPAccident>;
}

#endif//ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVEXERPACCIDENT_HXX

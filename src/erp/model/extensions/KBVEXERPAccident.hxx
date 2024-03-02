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
class KBVEXERPAccident : public Extension
{
public:
    using Extension::Extension;
    static constexpr auto url = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Accident";

    friend std::optional<KBVEXERPAccident> ResourceBase::getExtension<KBVEXERPAccident>(const std::string_view&) const;
};

}

#endif//ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVEXERPACCIDENT_HXX

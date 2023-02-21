/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVEXFORLEGALBASIS_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVEXFORLEGALBASIS_HXX

#include "erp/model/Extension.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KBVEXFORLegalBasis : public model::Extension
{
public:
    using Extension::Extension;
    static constexpr auto url = "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis";

    friend std::optional<KBVEXFORLegalBasis> ResourceBase::getExtension<KBVEXFORLegalBasis>(const std::string_view&) const;
};

}

#endif//ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVEXFORLEGALBASIS_HXX

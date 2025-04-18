/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVEXFORLEGALBASIS_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVEXFORLEGALBASIS_HXX

#include "shared/model/Extension.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class KBVEXFORLegalBasis : public Extension<KBVEXFORLegalBasis>
{
public:
    using Extension::Extension;
    static constexpr auto url = "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis";
};

extern template class Extension<KBVEXFORLegalBasis>;
extern template class Resource<KBVEXFORLegalBasis>;
}

#endif//ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_KBVEXFORLEGALBASIS_HXX

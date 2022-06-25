/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_CHARGEITEMMARKINGFLAG_H
#define ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_CHARGEITEMMARKINGFLAG_H

#include "erp/model/Extension.hxx"

#include <map>
#include <string>


namespace model
{

class ChargeItemMarkingFlag : public model::Extension
{
public:
    using Extension::Extension;

    static constexpr auto url = "https://gematik.de/fhir/StructureDefinition/MarkingFlag";

    using MarkingContainer = std::map<std::string, bool>;

    MarkingContainer getAllMarkings() const;

    static bool isMarked(const MarkingContainer& markings);

    friend std::optional<ChargeItemMarkingFlag> ResourceBase::getExtension<ChargeItemMarkingFlag>(const std::string_view&) const;
};

}

#endif

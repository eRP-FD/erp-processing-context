/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_CHARGEITEMMARKINGFLAG_H
#define ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_CHARGEITEMMARKINGFLAG_H

#include "erp/model/Extension.hxx"
#include "erp/model/ResourceNames.hxx"

#include <map>
#include <string>


namespace model
{

class Parameters;

// NOLINTNEXTLINE(bugprone-exception-escape)
class ChargeItemMarkingFlags : public model::Extension
{
public:
    using Extension::Extension;

    using MarkingContainer = std::map<std::string, bool>;
    static constexpr auto url = model::resource::structure_definition::markingFlag;
    static constexpr auto allMarkingFlags = {"insuranceProvider", "subsidy", "taxOffice"};

    MarkingContainer getAllMarkings() const;

    static bool isMarked(const MarkingContainer& markings);

    friend std::optional<ChargeItemMarkingFlags>
    ResourceBase::getExtension<ChargeItemMarkingFlags>(const std::string_view&) const;
    friend class Parameters;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
class ChargeItemMarkingFlag : public model::Extension
{
public:
    using Extension::Extension;
    ChargeItemMarkingFlag(const std::string_view url, bool value);

    friend std::optional<ChargeItemMarkingFlag>
    ResourceBase::getExtension<ChargeItemMarkingFlag>(const std::string_view&) const;
};

}

#endif

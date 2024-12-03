/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_CHARGEITEMMARKINGFLAG_H
#define ERP_PROCESSING_CONTEXT_MODEL_EXTENSIONS_CHARGEITEMMARKINGFLAG_H

#include "shared/model/Extension.hxx"
#include "shared/model/ResourceNames.hxx"

#include <map>
#include <string>


namespace model
{
template<typename>
class Parameters;

// NOLINTNEXTLINE(bugprone-exception-escape)
class ChargeItemMarkingFlags : public model::Extension<ChargeItemMarkingFlags>
{
public:
    using Extension::Extension;

    using MarkingContainer = std::map<std::string, bool>;
    static constexpr auto url = model::resource::structure_definition::markingFlag;
    static constexpr auto allMarkingFlags = {"insuranceProvider", "subsidy", "taxOffice"};

    MarkingContainer getAllMarkings() const;

    static bool isMarked(const MarkingContainer& markings);

    template<typename T>
    friend class Parameters;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
class ChargeItemMarkingFlag : public model::Extension<ChargeItemMarkingFlag>
{
public:
    using Extension::Extension;
    ChargeItemMarkingFlag(const std::string_view url, bool value);
};

extern template class Extension<ChargeItemMarkingFlags>;
extern template class Extension<ChargeItemMarkingFlag>;
extern template class Resource<ChargeItemMarkingFlags>;
extern template class Resource<ChargeItemMarkingFlag>;
}


#endif

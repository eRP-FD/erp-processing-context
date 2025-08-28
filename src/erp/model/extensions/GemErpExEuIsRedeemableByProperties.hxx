// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once
#include "shared/model/Extension.hxx"


namespace model
{
class GemErpExEuIsRedeemableByProperties : public Extension<GemErpExEuIsRedeemableByProperties>
{
public:
    using Extension::Extension;
    static constexpr auto url = model::resource::structure_definition::gem_erp_ex_eu_is_redeemable_by_properties;
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Extension<GemErpExEuIsRedeemableByProperties>;
extern template class Resource<GemErpExEuIsRedeemableByProperties>;
// NOLINTEND(bugprone-exception-escape)

}

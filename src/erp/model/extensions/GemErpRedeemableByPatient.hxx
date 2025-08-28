/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef GEMERPREDEEMABLEBYPATIENT_HXX
#define GEMERPREDEEMABLEBYPATIENT_HXX

#include "shared/model/Extension.hxx"

namespace model {

class GemErpRedeemableByPatient : public Extension<GemErpRedeemableByPatient>
{
public:
    using Extension::Extension;
    static constexpr auto url = model::resource::structure_definition::gem_erp_ex_eu_is_redeemable_by_patient_authorization;
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Extension<GemErpRedeemableByPatient>;
extern template class Resource<GemErpRedeemableByPatient>;
// NOLINTEND(bugprone-exception-escape)

} // model

#endif //GEMERPREDEEMABLEBYPATIENT_HXX

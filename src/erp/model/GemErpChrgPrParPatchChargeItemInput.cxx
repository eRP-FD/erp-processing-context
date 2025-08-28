// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/model/GemErpChrgPrParPatchChargeItemInput.hxx"

namespace model
{

ChargeItemMarkingFlags GemErpChrgPrParPatchChargeItemInput::createMarkingExtension() const
{
    std::optional<bool> insuranceProvider;
    std::optional<bool> subsidy;
    std::optional<bool> taxOffice;
    const auto& parameter = findParameter("markingFlag");
    ModelExpect(parameter, "markingFlag parameter not found");
    if (const auto& insuranceProviderPart = findPart(*parameter, "insuranceProvider"))
    {
        insuranceProvider = getValueBoolean(*insuranceProviderPart);
    }
    if (const auto& subsidyPart = findPart(*parameter, "subsidy"))
    {
        subsidy = getValueBoolean(*subsidyPart);
    }
    if (const auto& taxOfficePart = findPart(*parameter, "taxOffice"))
    {
        taxOffice = getValueBoolean(*taxOfficePart);
    }
    ModelExpect(insuranceProvider||subsidy||taxOffice, "no marking part found in markingFlag parameter");
    return ChargeItemMarkingFlags{insuranceProvider, subsidy, taxOffice};
}

template class Parameters<GemErpChrgPrParPatchChargeItemInput>;
}

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/model/Parameters.hxx"

#include <utility>
#include <vector>

#include "shared/model/GemErpEuPrPractitioner.hxx"
#include "shared/model/GemErpEuPrPractitionerRole.hxx"
#include "shared/model/GemErpEuPrOrganization.hxx"

namespace model
{

class CountryCode;
class EuAccessCode;
class Kvnr;

class GemErpEuPrParCloseOperationInput : public Parameters<GemErpEuPrParCloseOperationInput>
{
public:
    static constexpr auto profileType = ProfileType::GEM_ERPEU_PR_PAR_CloseOperation_Input;
    using Parameters::Parameters;
    using Parameters::findResourceParameter;
    friend class Resource;

    Kvnr kvnr() const;
    EuAccessCode accessCode() const;
    CountryCode countryCode() const;
    std::string_view practitionerName() const;
    std::string_view practitionerRole() const;
    std::string_view pointOfCare() const;
    std::string_view healthcareFacilityType() const;
    std::string practitionerId(const std::string& practitionerRef) const;
    std::string practitionerReference(const std::string& practitionerRoleRef) const;

    model::GemErpEuPrPractitioner practitionerModel() const;
    model::GemErpEuPrPractitionerRole practitionerRoleModel() const;
    model::GemErpEuPrOrganization organizationModel() const;

private:
    const rapidjson::Value& getPart(const std::string& parameterName, const std::string& partName) const;
    template <typename T> T getModel(const std::string& paramName) const
    {
        const auto* parameter = findParameter(paramName);
        ModelExpect(parameter, paramName + " parameter not found in GEM_ERPEU_PR_PAR_CloseOperation_Input");
        return T::fromJson(getResourceDoc(*parameter));
    }
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Resource<GemErpEuPrParCloseOperationInput>;
extern template class Parameters<GemErpEuPrParCloseOperationInput>;
// NOLINTEND

}// model

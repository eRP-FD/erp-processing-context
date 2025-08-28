/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/eu/GemErpEuPrParCloseOperationInput.hxx"
#include "erp/model/eu/EuAccessCode.hxx"
#include "shared/model/CountryCode.hxx"
#include "shared/model/GemErpEuPrPractitioner.hxx"
#include "shared/model/GemErpEuPrPractitionerRole.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/util/String.hxx"
#include "shared/util/Uuid.hxx"

namespace model
{

namespace
{

std::string stringFromUuidOrRef(const std::string& src)
{
    std::string val{};
    if (Uuid::isValidUrnUuid(src))
    {
        val = Uuid{src}.toString();
    }
    else
    {
        const size_t pos = src.rfind('/');
        if (pos != std::string::npos)
        {
            val = src.substr(pos + 1);
        }
    }
    return val;
}

}// anon

const rapidjson::Value& GemErpEuPrParCloseOperationInput::getPart(const std::string& parameterName,
                                                                  const std::string& partName) const
{
    const auto* parameter = findParameter(parameterName);
    ModelExpect(parameter, parameterName + " parameter not found in GEM_ERPEU_PR_PAR_CloseOperation_Input");
    const auto* part = findPart(*parameter, partName);
    ModelExpect(part, partName + " part not found in GEM_ERPEU_PR_PAR_CloseOperation_Input");
    return *part;
}

Kvnr GemErpEuPrParCloseOperationInput::kvnr() const
{
    std::string strVal{getValueIdentifier(getPart("requestData", "kvnr"))};
    Kvnr kvnr{strVal};
    ModelExpect(kvnr.validFormat(), strVal + ": invalid kvnr parameter found in GEM_ERPEU_PR_PAR_CloseOperation_Input");
    return kvnr;
}

EuAccessCode GemErpEuPrParCloseOperationInput::accessCode() const
{
    SafeString strVal{std::string{getValueIdentifier(getPart("requestData", "accessCode"))}};
    return EuAccessCode{std::move(strVal)};
}

CountryCode GemErpEuPrParCloseOperationInput::countryCode() const
{
    std::string strVal{getValueCoding(getPart("requestData", "countryCode"))};
    return CountryCode{strVal};
}

std::string_view GemErpEuPrParCloseOperationInput::practitionerName() const
{
    return getValueString(getPart("requestData", "practitionerName"));
}

    std::string_view GemErpEuPrParCloseOperationInput::practitionerRole() const
{
    return getValueCoding(getPart("requestData", "practitionerRole"));
}

std::string GemErpEuPrParCloseOperationInput::practitionerId(const std::string& practitionerRef) const
{
    std::string val = stringFromUuidOrRef(practitionerRef);

    auto validatedModel = getModel<model::GemErpEuPrPractitioner>("practitionerData");
    ModelExpect(validatedModel.getId().value() == val, "Mismatched practitioner id.");

    return std::string{validatedModel.practitionerId()};
}

std::string GemErpEuPrParCloseOperationInput::practitionerReference(const std::string& practitionerRoleRef) const {
    std::string val = stringFromUuidOrRef(practitionerRoleRef);

    auto validatedModel = getModel<model::GemErpEuPrPractitionerRole>("practitionerRoleData");
    ModelExpect(validatedModel.getId().value() == val, "Mismatched practitioner role id.");

    return std::string{validatedModel.practitionerReference()};
}

std::string_view GemErpEuPrParCloseOperationInput::pointOfCare() const
{
    return getValueString(getPart("requestData", "pointOfCare"));
}

std::string_view GemErpEuPrParCloseOperationInput::healthcareFacilityType() const
{
    return getValueCoding(getPart("requestData", "healthcare-facility-type"));
}

model::GemErpEuPrPractitioner GemErpEuPrParCloseOperationInput::practitionerModel() const
{
    return getModel<model::GemErpEuPrPractitioner>("practitionerData");
}

model::GemErpEuPrPractitionerRole GemErpEuPrParCloseOperationInput::practitionerRoleModel() const
{
    return getModel<model::GemErpEuPrPractitionerRole>("practitionerRoleData");
}

model::GemErpEuPrOrganization GemErpEuPrParCloseOperationInput::organizationModel() const
{
    return getModel<model::GemErpEuPrOrganization>("organizationData");
}

template class Parameters<GemErpEuPrParCloseOperationInput>;

}

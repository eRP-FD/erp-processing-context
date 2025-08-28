// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/model/eu/GemErpEuPrParGetPrescriptionInput.hxx"

namespace model
{


std::string GemErpEuPrParGetPrescriptionInput::requestTypeToString(RequestType requestType)
{
    switch (requestType)
    {
        case RequestType::demographics:
        case RequestType::e_prescriptions_list:
        case RequestType::e_prescriptions_retrieval:
            return gsl::at(RequestTypeNames, gsl::narrow<size_t>(magic_enum::enum_integer(requestType)));
    }
    ModelFail("invalid requestType: " + std::to_string(static_cast<uintmax_t>(requestType)));
}

GemErpEuPrParGetPrescriptionInput::RequestType
GemErpEuPrParGetPrescriptionInput::requestTypeFromString(std::string_view str)
{
    for (size_t i = 0; i < RequestTypeNames.size(); ++i)
    {
        if (str == gsl::at(RequestTypeNames, i))
        {
            return magic_enum::enum_value<RequestType>(i);
        }
    }
    ModelFail("invalid requestType: " + std::string{str});
}

GemErpEuPrParGetPrescriptionInput::RequestType GemErpEuPrParGetPrescriptionInput::requestType() const
{
    std::string strVal{getValueCoding(getPart("requesttype"))};
    auto requestTypeStr = requestTypeFromString(strVal);
    return requestTypeStr;
}

Kvnr GemErpEuPrParGetPrescriptionInput::kvnr() const
{
    std::string strVal{getValueIdentifier(getPart("kvnr"))};
    Kvnr kvnr{strVal};
    ModelExpect(kvnr.validFormat(), strVal + ": invalid kvnr parameter found in GEM_ERPEU_PR_PAR_GetPrescriptionInput");
    return kvnr;
}

EuAccessCode GemErpEuPrParGetPrescriptionInput::accessCode() const
{
    SafeString strVal{std::string{getValueIdentifier(getPart("accessCode"))}};
    return EuAccessCode{std::move(strVal)};
}

CountryCode GemErpEuPrParGetPrescriptionInput::countryCode() const
{
    std::string strVal{getValueCoding(getPart("countryCode"))};
    return CountryCode{strVal};
}

std::vector<PrescriptionId> GemErpEuPrParGetPrescriptionInput::prescriptionIds() const
{
    const auto* parameter = findParameter("requestData");
    ModelExpect(parameter, "requestData parameter not found in GEM_ERPEU_PR_PAR_GetPrescriptionInput");
    auto parts = findParts(*parameter, "prescription-id");
    std::vector<PrescriptionId> prescriptionIds;
    prescriptionIds.reserve(parts.size());
    for (const auto* part : parts)
    {
        prescriptionIds.emplace_back(PrescriptionId::fromString(getValueIdentifier(*part)));
    }
    return prescriptionIds;
}

std::string_view GemErpEuPrParGetPrescriptionInput::practitionerName() const
{
    return getValueString(getPart("practitionerName"));
}

std::string_view GemErpEuPrParGetPrescriptionInput::practitionerRole() const
{
    return getValueCoding(getPart("practitionerRole"));
}

std::string_view GemErpEuPrParGetPrescriptionInput::pointOfCare() const
{
    return getValueString(getPart("pointOfCare"));
}

std::string_view GemErpEuPrParGetPrescriptionInput::healthcareFacilityType() const
{
    return getValueCoding(getPart("healthcare-facility-type"));
}

const rapidjson::Value& GemErpEuPrParGetPrescriptionInput::getPart(const std::string& parameterName) const
{
    const auto* parameter = findParameter("requestData");
    ModelExpect(parameter, "requestData parameter not found in GEM_ERPEU_PR_PAR_GetPrescriptionInput");
    const auto* part = findPart(*parameter, parameterName);
    ModelExpect(part, parameterName + " part not found in GEM_ERPEU_PR_PAR_GetPrescriptionInput");
    return *part;
}

template class Parameters<GemErpEuPrParGetPrescriptionInput>;
}

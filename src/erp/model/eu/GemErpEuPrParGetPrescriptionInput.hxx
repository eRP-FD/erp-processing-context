// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef ERP_PROCESSING_CONTEXT_MODEL_GEMERPEUPRPARGETPRESCRIPTIONINPUT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_GEMERPEUPRPARGETPRESCRIPTIONINPUT_HXX

#include "erp/model/eu/EuAccessCode.hxx"
#include "erp/util/search/SearchParameter.hxx"
#include "shared/model/CountryCode.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/Parameters.hxx"
#include "shared/model/PrescriptionId.hxx"

namespace model
{


class GemErpEuPrParGetPrescriptionInput : public Parameters<GemErpEuPrParGetPrescriptionInput>
{
public:
    static constexpr auto profileType = ProfileType::GEM_ERPEU_PR_PAR_GET_Prescription_Input;
    using Parameters::Parameters;
    friend class Resource;

    enum class RequestType
    {
        demographics = 0,
        e_prescriptions_list,
        e_prescriptions_retrieval
    };
    static constexpr std::array<const char*, 3> RequestTypeNames = {"demographics", "e-prescriptions-list",
                                                                    "e-prescriptions-retrieval"};
    static std::string requestTypeToString(RequestType requestType);
    static RequestType requestTypeFromString(std::string_view str);

    [[nodiscard]] RequestType requestType() const;
    [[nodiscard]] Kvnr kvnr() const;
    [[nodiscard]] EuAccessCode accessCode() const;
    [[nodiscard]] CountryCode countryCode() const;
    [[nodiscard]] std::vector<PrescriptionId> prescriptionIds() const;
    [[nodiscard]] std::string_view practitionerName() const;
    [[nodiscard]] std::string_view practitionerRole() const;
    [[nodiscard]] std::string_view pointOfCare() const;
    [[nodiscard]] std::string_view healthcareFacilityType() const;

private:
    const rapidjson::Value& getPart(const std::string& parameterName) const;
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Resource<GemErpEuPrParGetPrescriptionInput>;
extern template class Parameters<GemErpEuPrParGetPrescriptionInput>;
// NOLINTEND
}// model

#endif//ERP_PROCESSING_CONTEXT_MODEL_GEMERPEUPRPARGETPRESCRIPTIONINPUT_HXX

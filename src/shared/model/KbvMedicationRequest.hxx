/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVMEDICATIONREQUEST_HXX_
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVMEDICATIONREQUEST_HXX_

#include "shared/model/Resource.hxx"

namespace model
{

class Dosage;

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvMedicationRequest : public Resource<KbvMedicationRequest>
{
public:
    static constexpr auto resourceTypeName = "MedicationRequest";
    using Resource::Resource;
    [[nodiscard]] std::optional<std::string_view> statusCoPaymentExtension() const;
    bool isMultiplePrescription() const;
    [[nodiscard]] std::optional<date::year_month_day> mvoEndDate() const;

    std::optional<Dosage> dosageInstruction() const;
    [[nodiscard]] model::Timestamp authoredOn() const;

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;

private:
    friend Resource<KbvMedicationRequest>;
    explicit KbvMedicationRequest(NumberAsStringParserDocument&& document);
};

class Dosage : public Resource<Dosage>
{
public:
    static constexpr auto resourceTypeName = "Dosage";
    [[nodiscard]] std::optional<std::string_view> text() const;

private:
    friend Resource<Dosage>;
    explicit Dosage(NumberAsStringParserDocument&& document);
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Resource<KbvMedicationRequest>;
extern template class Resource<Dosage>;
// NOLINTEND(bugprone-exception-escape)
}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVMEDICATIONREQUEST_HXX_

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVMEDICATIONREQUEST_HXX_
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVMEDICATIONREQUEST_HXX_

#include "erp/database/Database.hxx"
#include "erp/model/Resource.hxx"

namespace model
{

class Dosage;

// NOLINTNEXTLINE(bugprone-exception-escape)
class KbvMedicationRequest : public Resource<KbvMedicationRequest, ResourceVersion::KbvItaErp>
{
public:
    static constexpr auto resourceTypeName = "MedicationRequest";
    using Resource::Resource;
    [[nodiscard]] std::optional<std::string_view> statusCoPaymentExtension() const;
    bool isMultiplePrescription() const;
    [[nodiscard]] std::optional<date::year_month_day> mvoEndDate() const;

    std::optional<Dosage> dosageInstruction() const;
    [[nodiscard]] model::Timestamp authoredOn() const;

private:
    friend Resource<KbvMedicationRequest, ResourceVersion::KbvItaErp>;
    explicit KbvMedicationRequest(NumberAsStringParserDocument&& document);
};

class Dosage : public Resource<Dosage, ResourceVersion::KbvItaErp>
{
public:
    static constexpr auto resourceTypeName = "Dosage";
    [[nodiscard]] std::optional<std::string_view> text() const;

private:
    friend Resource<Dosage, ResourceVersion::KbvItaErp>;
    explicit Dosage(NumberAsStringParserDocument&& document);
};

}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_KBVMEDICATIONREQUEST_HXX_

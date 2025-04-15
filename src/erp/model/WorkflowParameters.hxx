/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_WORKFLOW_PARAMETER_HXX
#define ERP_PROCESSING_CONTEXT_WORKFLOW_PARAMETER_HXX

#include "erp/model/extensions/ChargeItemMarkingFlags.hxx"
#include "shared/model/Parameters.hxx"
#include "shared/model/PrescriptionType.hxx"
#include "shared/model/Resource.hxx"

#include <string_view>

namespace model
{

class CreateTaskParameters : public Parameters<CreateTaskParameters>
{
public:
    static constexpr auto profileType = ProfileType::CreateTaskParameters;
    using Parameters::Parameters;
    friend class Resource;

    std::optional<model::PrescriptionType> getPrescriptionType() const;

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;
};

class ActivateTaskParameters : public Parameters<ActivateTaskParameters>
{
public:
    static constexpr auto profileType = ProfileType::ActivateTaskParameters;
    using Parameters::Parameters;
    friend class Resource;

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;
};

class PatchChargeItemParameters : public Parameters<PatchChargeItemParameters>
{
public:
    static constexpr auto profileType = ProfileType::PatchChargeItemParameters;
    using Parameters::Parameters;
    friend class Resource;

    struct MarkingFlag
    {
        struct Element
        {
            std::string_view url;
            std::optional<bool> value;
        };

        Element insuranceProvider;
        Element subsidy;
        Element taxOffice;

        MarkingFlag();

        std::optional<std::reference_wrapper<Element>> findByUrl(const std::string_view& url);

        std::string toExtensionJson() const;
    };

    // Available in PATCH ChargeItem
    model::ChargeItemMarkingFlags getChargeItemMarkingFlags() const;

    std::optional<Timestamp> getValidationReferenceTimestamp() const override;

private:
    void updateMarkingFlagFromPartArray(
        const NumberAsStringParserDocument::ConstArray& partArray,
        MarkingFlag& result) const;
};

// NOLINTBEGIN(bugprone-exception-escape)
extern template class Resource<CreateTaskParameters>;
extern template class Parameters<CreateTaskParameters>;
extern template class Resource<class ActivateTaskParameters>;
extern template class Parameters<ActivateTaskParameters>;
extern template class Resource<PatchChargeItemParameters>;
extern template class Parameters<PatchChargeItemParameters>;
// NOLINTEND(bugprone-exception-escape)
}

#endif //ERP_PROCESSING_CONTEXT_WORKFLOW_PARAMETER_HXX

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_PARAMETER_HXX
#define ERP_PROCESSING_CONTEXT_PARAMETER_HXX

#include "erp/model/PrescriptionType.hxx"
#include "erp/model/Resource.hxx"
#include "erp/model/extensions/ChargeItemMarkingFlags.hxx"

#include <libxml2/libxml/tree.h>
#include <memory>
#include <string_view>


namespace model
{
// NOLINTNEXTLINE(bugprone-exception-escape)
template<typename ParametersT>
class Parameters : public Resource<ParametersT>
{
public:
    static constexpr auto resourceTypeName = "Parameters";

    size_t count() const;
    const rapidjson::Value* findResourceParameter(std::string_view name) const;
    bool hasParameter(std::string_view name) const;

    std::optional<std::string_view> getWorkflowSystem() const;


private:
    friend class Resource<ParametersT>;
    using Resource<ParametersT>::Resource;
};

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

#endif //ERP_PROCESSING_CONTEXT_PARAMETER_HXX

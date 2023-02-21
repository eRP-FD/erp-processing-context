/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
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
class Parameters : public Resource<Parameters, ResourceVersion::NotProfiled>
{
public:
    static constexpr auto resourceTypeName = "Parameters";

    size_t count() const;
    const rapidjson::Value* findResourceParameter(std::string_view name) const;
    bool hasParameter(std::string_view name) const;

    // Available in Task/$create request parameter
    std::optional<model::PrescriptionType> getPrescriptionType() const;
    std::optional<std::string_view> getWorkflowSystem() const;

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

private:
    friend Resource;
    explicit Parameters (NumberAsStringParserDocument&& document);

    void updateMarkingFlagFromPartArray(
        const NumberAsStringParserDocument::ConstArray& partArray,
        MarkingFlag& result) const;
};


}

#endif //ERP_PROCESSING_CONTEXT_PARAMETER_HXX

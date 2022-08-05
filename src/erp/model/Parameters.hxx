/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_PARAMETER_HXX
#define ERP_PROCESSING_CONTEXT_PARAMETER_HXX

#include "erp/model/PrescriptionType.hxx"
#include "erp/model/Resource.hxx"
#include "erp/model/extensions/ChargeItemMarkingFlag.hxx"

#include <libxml2/libxml/tree.h>

#include <memory>
#include <string_view>


namespace model
{
class Parameters : public Resource<Parameters, ResourceVersion::NotProfiled>
{
public:
    static constexpr auto resourceTypeName = "Parameters";

    size_t count() const;
    const rapidjson::Value* findResourceParameter(const std::string_view& name) const;

    // Available in Task/$create request parameter
    std::optional<model::PrescriptionType> getPrescriptionType() const;

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
    std::optional<model::ChargeItemMarkingFlag> getChargeItemMarkingFlag() const;
    void updateMarkingFlagFromPart(
        const NumberAsStringParserDocument::ConstArray& partArray,
        MarkingFlag& result) const;

private:
    friend Resource;
    explicit Parameters (NumberAsStringParserDocument&& document);
};


}

#endif //ERP_PROCESSING_CONTEXT_PARAMETER_HXX

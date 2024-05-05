/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef FHIR_TOOLS_SRC_FHIR_PATH_MODEL_ERP_ERPELEMENT_HXX
#define FHIR_TOOLS_SRC_FHIR_PATH_MODEL_ERP_ERPELEMENT_HXX


#include <rapidjson/fwd.h>
#include <optional>
#include <variant>

#include "fhirtools/model/Element.hxx"

/// @brief This class implements the FHIR-Validator data model for the ERP-model
///        that is based on rapidjson (NumberAsStringParserDocument)
class ErpElement : public fhirtools::Element
{
public:
    explicit ErpElement(const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
                        std::weak_ptr<const Element> parent, const std::string& elementId,
                        const rapidjson::Value* value, const rapidjson::Value* primitiveTypeObject = nullptr);
    explicit ErpElement(const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
                        std::weak_ptr<const Element> parent, fhirtools::ProfiledElementTypeInfo defPtr,
                        const rapidjson::Value* value, const rapidjson::Value* primitiveTypeObject = nullptr);
    explicit ErpElement(const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
                        std::weak_ptr<const Element> parent,
                        const fhirtools::FhirStructureDefinition* structureDefinition, const std::string& elementId,
                        const rapidjson::Value* value, const rapidjson::Value* primitiveTypeObject = nullptr);

    [[nodiscard]] std::string resourceType() const override;
    [[nodiscard]] std::vector<std::string_view> profiles() const override;

    [[nodiscard]] int32_t asInt() const override;
    [[nodiscard]] fhirtools::DecimalType asDecimal() const override;
    [[nodiscard]] bool asBool() const override;
    [[nodiscard]] std::string asString() const override;
    [[nodiscard]] fhirtools::Date asDate() const override;
    [[nodiscard]] fhirtools::Time asTime() const override;
    [[nodiscard]] fhirtools::DateTime asDateTime() const override;
    [[nodiscard]] QuantityType asQuantity() const override;

    [[nodiscard]] bool hasSubElement(const std::string& name) const override;
    [[nodiscard]] std::vector<std::string> subElementNames() const override;
    [[nodiscard]] bool hasValue() const override;
    [[nodiscard]] std::vector<std::shared_ptr<const Element>> subElements(const std::string& name) const override;
    [[nodiscard]] const rapidjson::Value* erpValue() const;

private:
    fhirtools::PrimitiveElement asPrimitiveElement() const;
    static std::string resourceType(const rapidjson::Value& resource);
    static std::vector<std::string_view> profiles(const rapidjson::Value& resource);
    std::shared_ptr<ErpElement> createElement(fhirtools::ProfiledElementTypeInfo, bool isResource,
                                              const rapidjson::Value* val, const rapidjson::Value* primitiveVal) const;
    std::vector<std::shared_ptr<const Element>> arraySubElements(const fhirtools::ProfiledElementTypeInfo& subPtr,
                                                                 const rapidjson::Value* val,
                                                                 const rapidjson::Value* primitiveVal,
                                                                 const std::string& name) const;

    const rapidjson::Value* mValue;
    const rapidjson::Value* mPrimitiveTypeObject;
    using SubElementCache = std::map<std::string, std::vector<std::shared_ptr<const Element>>>;
    mutable SubElementCache mSubElementCache;
    // primitive types can have substructures, denoted by _
    // e.g.:
    /*
    "family":"Ludger Königsstein",
    "_family":{
        "extension":[
            {
                "url":"http://hl7.org/fhir/StructureDefinition/humanname-own-name",
                "valueString":"Königsstein"
            }
        ]
    }
    */
};


#endif//FHIR_TOOLS_SRC_FHIR_PATH_MODEL_ERP_ERPELEMENT_HXX

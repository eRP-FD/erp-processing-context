/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef FHIR_TOOLS_SRC_FHIR_PATH_MODEL_ERP_ERPELEMENT_HXX
#define FHIR_TOOLS_SRC_FHIR_PATH_MODEL_ERP_ERPELEMENT_HXX


#include "fhirtools/model/MutableElement.hxx"

#include <gsl/gsl-lite.hpp>
#include <rapidjson/fwd.h>
#include <optional>
#include <variant>

namespace model
{
class NumberAsStringParserDocument;
}

/// @brief This class implements the FHIR-Validator data model for the ERP-model
///        that is based on rapidjson (NumberAsStringParserDocument)
class ErpElement : public fhirtools::MutableElement
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

    explicit ErpElement(const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
                        std::weak_ptr<const Element> parent, fhirtools::ProfiledElementTypeInfo defPtr,
                        model::NumberAsStringParserDocument* document, rapidjson::Value* value,
                        rapidjson::Value* primitiveTypeObject);

    [[nodiscard]] std::string resourceType() const override;
    [[nodiscard]] std::vector<std::string_view> profiles() const override;
    [[nodiscard]] std::string asRaw() const override;
    [[nodiscard]] int32_t asInt() const override;
    [[nodiscard]] fhirtools::DecimalType asDecimal() const override;
    [[nodiscard]] bool asBool() const override;
    [[nodiscard]] std::string asString() const override;
    [[nodiscard]] fhirtools::Date asDate() const override;
    [[nodiscard]] fhirtools::Time asTime() const override;
    [[nodiscard]] fhirtools::DateTime asDateTime() const override;
    [[nodiscard]] QuantityType asQuantity() const override;
    [[nodiscard]] const rapidjson::Value* asJson() const;

    [[nodiscard]] bool hasSubElement(const std::string& name) const override;
    [[nodiscard]] std::vector<std::string> subElementNames() const override;
    [[nodiscard]] bool hasValue() const override;
    [[nodiscard]] std::vector<std::shared_ptr<const Element>> subElements(const std::string& name) const override;

    // MutableElement
    [[nodiscard]] const std::string& elementName() const override;
    void setString(std::string_view stringValue) const override;
    void setDataAbsentExtension(const std::string_view& missingElementName) const override;
    void removeFromParent() const override;

private:
    fhirtools::PrimitiveElement asPrimitiveElement() const;
    static std::string resourceType(const rapidjson::Value& resource);
    static std::vector<std::string_view> profiles(const rapidjson::Value& resource);
    std::shared_ptr<ErpElement> createElement(fhirtools::ProfiledElementTypeInfo, bool isResource,
                                              const rapidjson::Value* val, const rapidjson::Value* primitiveVal,
                                              rapidjson::Value* valMutable, rapidjson::Value* primitiveValMutable,
                                              const std::string& elementName, std::optional<size_t> arrayIndex) const;
    template<typename ValueT>
    std::vector<std::shared_ptr<const Element>> arraySubElements(const fhirtools::ProfiledElementTypeInfo& subPtr,
                                                                 ValueT* val, ValueT* primitiveVal,
                                                                 const std::string& name) const;
    void removeSubElement(const std::string& name, std::optional<size_t> arrayIndex) const;

    const rapidjson::Value* mValue;
    const rapidjson::Value* mPrimitiveTypeObject;
    rapidjson::Value* mValueMutable = nullptr;
    rapidjson::Value* mPrimitiveTypeObjectMutable = nullptr;
    model::NumberAsStringParserDocument* mDocumentMutable = nullptr;
    using SubElementCache = std::map<std::string, std::vector<std::shared_ptr<const Element>>>;
    mutable SubElementCache mSubElementCache;
    std::string mElementName;
    mutable std::optional<size_t> mArrayIndex = 0;
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

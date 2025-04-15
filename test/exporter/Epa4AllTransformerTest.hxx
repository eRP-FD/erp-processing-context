/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */
#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_FHIR_EPA4ALLTRANSFORMERTEST_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_FHIR_EPA4ALLTRANSFORMERTEST_HXX

#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/TelematikId.hxx"
#include "shared/model/Timestamp.hxx"

#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/rapidjson.h>

struct MappedValue {
    std::string property;
    std::string targetValue;
    bool canBeMissing{false};
};

struct ExtensionMapping {
    std::string sourceUrl;
    std::string targetUrl;
    std::vector<MappedValue> mappedValues;
    std::vector<std::string> retainedValues;
    std::vector<std::string> emptyInTarget;
    bool found = false;
};


class Epa4AllTransformerTest : public testing::Test
{
public:
    void checkMedicationRequest(const rapidjson::Value& source, const rapidjson::Value& target,
                                const model::Kvnr& kvnr);
    void checkMedicationCompounding(const rapidjson::Value& source, const rapidjson::Value& target);
    void checkMedicationFreeText(const rapidjson::Value& source, const rapidjson::Value& target);
    void checkMedicationIngredient(const rapidjson::Value& source, const rapidjson::Value& target);
    void checkMedicationPzn(const rapidjson::Value& source, const rapidjson::Value& target);
    void checkOrganization(const rapidjson::Value& source, const rapidjson::Value& target);
    void checkPractitioner(const rapidjson::Value& source, const rapidjson::Value& target);

    void checkRetainedProperties(const rapidjson::Value& source, const rapidjson::Value& target,
                                 const std::vector<std::string>& properties);

    void checkPropertiesNotInTarget(const rapidjson::Value& target, const std::vector<std::string>& properties);

    void checkMappedValues(const rapidjson::Value& target, const std::vector<MappedValue>& mappings);

    void checkExtensions(const rapidjson::Value& source, const rapidjson::Value& target,
                         std::vector<ExtensionMapping>& mappings);
    void checkExtensionRemoved(const rapidjson::Value& source, const rapidjson::Value& target,
                               const std::string& extensionUrl, bool forceSource);
    void checkIdentifier(const rapidjson::Value& resource, const std::string_view& expectedSystem,
                         const std::string_view& expectedValue);
    void checkMedicationIngredientArray(const rapidjson::Value& source, const rapidjson::Value& target,
                                        bool checkTransformedPzn);
    void checkMedicationContainedArray(const rapidjson::Value& source, const rapidjson::Value& target);
    void checkMedicationCodingArray(const rapidjson::Value& source, const rapidjson::Value& target);


    rapidjson::Pointer makePointer(std::string_view property);

    std::string serialize(const rapidjson::Value& value);

    const rapidjson::Value* findExtension(const rapidjson::Value& where, const std::string& url);

    std::string str(const rapidjson::Value& value);

    void validate(fhirtools::DefinitionKey targetProfileKey, const rapidjson::Value* resource,
                  const model::Timestamp& timestamp = model::Timestamp::now());
    void checkExpression(const std::string& expression, fhirtools::DefinitionKey targetProfileKey,
                         const rapidjson::Value* resource);
    std::shared_ptr<ErpElement> createErpElement(fhirtools::DefinitionKey targetProfileKey,
                                                 const rapidjson::Value* resource, const model::Timestamp& timestamp);
    std::shared_ptr<fhirtools::FhirStructureRepository> getRepo(fhirtools::DefinitionKey targetProfileKey,
                                                                const model::Timestamp& timestamp);

    const model::TelematikId telematikIdFromQes{"not-set"};
    const model::TelematikId telematikIdFromAccessToken{"not-set-2"};
    constexpr static const std::string organizationNameFromJwt = "not set";
};


#endif//ERP_PROCESSING_CONTEXT_TEST_ERP_FHIR_EPA4ALLTRANSFORMERTEST_HXX

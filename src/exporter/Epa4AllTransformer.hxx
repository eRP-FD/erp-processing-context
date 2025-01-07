/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_FHIR_EPA4ALLTRANSFORMER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_FHIR_EPA4ALLTRANSFORMER_HXX

#include "exporter/model/EpaOpProvideDispensationErpInputParameters.hxx"
#include "exporter/model/EpaOpProvidePrescriptionErpInputParameters.hxx"

namespace model
{
class Bundle;
class Task;
class ResourceBase;
class KbvMedicationGeneric;
class TelematikId;
class Pzn;
class KbvMedicationCompounding;
}

namespace fhirtools
{
struct ValueMapping;
}

class Epa4AllTransformer
{
public:
    struct FixedValue {
        rapidjson::Pointer ptr;
        std::string value;
    };
    static model::EPAOpProvidePrescriptionERPInputParameters
    transformPrescription(const model::Bundle& kbvBundle, const model::TelematikId& telematikIdFromQes,
                          const model::TelematikId& telematikIdFromJwt, const std::string_view& organizationNameFromJwt,
                          const std::string& organizationProfessionOid);

    static model::EPAOpProvideDispensationERPInputParameters transformMedicationDispenseWithKBVMedication(
        const model::Bundle& medicationDispenseBundle, model::PrescriptionId prescriptionId,
        const model::Timestamp& authoredOn, const model::TelematikId& actorTelematikId,
        const std::string& actorOrganizationName, const std::string& organizationProfessionOid);

    static model::EPAOpProvideDispensationERPInputParameters
    transformMedicationDispense(const model::Bundle& medicationDispenseBundle,
                                const model::PrescriptionId& prescriptionId, const model::Timestamp& authoredOn,
                                const model::TelematikId& actorTelematikId, const std::string& actorOrganizationName,
                                const std::string& organizationProfessionOid);

    static std::string buildPractitionerName(const rapidjson::Value& humanName);

private:
    static model::EPAOpProvideDispensationERPInputParameters
    transformMedicationDispenseCommon(model::PrescriptionId prescriptionId, const model::Timestamp& authoredOn,
                                      const model::TelematikId& actorTelematikId,
                                      const std::string& actorOrganizationName,
                                      const std::string& organizationProfessionOid);
    static model::NumberAsStringParserDocument transformKbvMedication(const model::KbvMedicationGeneric& kbvMedication);
    static void convertPZNIngredients(model::NumberAsStringParserDocument& transformedMedication,
                                      const model::KbvMedicationCompounding& kbvMedicationCompounding);
    static void convertPZNIngredient(model::NumberAsStringParserDocument& transformedMedication,
                                     rapidjson::Value& epaIngredient, model::Pzn&& pzn, std::string_view text);
    static model::NumberAsStringParserDocument
    transformResource(const fhirtools::DefinitionKey& targetProfileKey, const model::FhirResourceBase& sourceResource,
                      const std::vector<fhirtools::ValueMapping>& valueMappings,
                      const std::vector<FixedValue>& fixedValues);
    static void removeMedicationCodeCodingsByAllowlist(rapidjson::Value& medicationResource);
    static void remove(rapidjson::Value& from, const rapidjson::Pointer& toBeRemoved);
    static void checkSetAbsentReason(model::NumberAsStringParserDocument& document, const std::string& memberPath);
    static void addToKeyValueArray(model::NumberAsStringParserDocument& document,
                                   const rapidjson::Pointer& arrayPointer, const rapidjson::Pointer& keyPtr,
                                   const std::string_view key, const rapidjson::Pointer& valuePtr,
                                   const std::string_view value, bool overwrite);
    static void addTelematikIdentifier(model::NumberAsStringParserDocument& document,
                                       const std::string_view& telematikIdFromQes, bool overwrite);
    static void addMetaTagOriginLdap(model::NumberAsStringParserDocument& document);
    static void addTypeProfessionOid(model::NumberAsStringParserDocument& document,
                                     const std::string& organizationProfessionOid);
    static std::shared_ptr<fhirtools::FhirStructureRepository> getRepo(const fhirtools::DefinitionKey& profileKey);
};

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_FHIR_EPA4ALLTRANSFORMER_HXX

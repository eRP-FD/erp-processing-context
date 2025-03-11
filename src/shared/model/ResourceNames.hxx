/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_RESOURCENAMES_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_RESOURCENAMES_HXX

#include "ProfileType.hxx"
#include "fhirtools/repository/FhirVersion.hxx"

#include <optional>
#include <string>
#include <string_view>

namespace fhirtools
{
struct DefinitionKey;
class FhirStructureRepository;
class FhirVersion;
}

namespace model
{
class Timestamp;


std::optional<fhirtools::DefinitionKey> profileWithVersion(ProfileType profileType,
                                                           const model::Timestamp& referenceTime);

std::optional<fhirtools::DefinitionKey> profileWithVersion(ProfileType profileType,
                                                           const fhirtools::FhirStructureRepository& repoView);

std::optional<std::string_view> profile(ProfileType profileType);
std::optional<ProfileType> findProfileType(std::string_view profileUrl);
std::string profileList(const fhirtools::FhirStructureRepository& repoView, std::initializer_list<ProfileType> types);

namespace version
{
static inline fhirtools::FhirVersion GEM_ERP_1_4{"1.4"};
}

namespace resource
{
namespace naming_system
{
constexpr std::string_view accessCode = "https://gematik.de/fhir/erp/NamingSystem/GEM_ERP_NS_AccessCode";
constexpr std::string_view argeIknr = "http://fhir.de/sid/arge-ik/iknr";
constexpr std::string_view gkvKvid10 = "http://fhir.de/sid/gkv/kvid-10";
constexpr std::string_view pkvKvid10 = "http://fhir.de/sid/pkv/kvid-10";
constexpr std::string_view prescriptionID = "https://gematik.de/fhir/erp/NamingSystem/GEM_ERP_NS_PrescriptionId";
constexpr std::string_view secret = "https://gematik.de/fhir/erp/NamingSystem/GEM_ERP_NS_Secret";
constexpr std::string_view telematicID = "https://gematik.de/fhir/sid/telematik-id";
constexpr std::string_view kbvAnr = "https://fhir.kbv.de/NamingSystem/KBV_NS_Base_ANR";
constexpr std::string_view kbvZanr = "http://fhir.de/sid/kzbv/zahnarztnummer";
constexpr std::string_view organizationProfessionOid =
    "https://gematik.de/fhir/directory/CodeSystem/OrganizationProfessionOID";
} // namespace naming_system

namespace code_system
{
constexpr std::string_view consentType = "https://gematik.de/fhir/erpchrg/CodeSystem/GEM_ERPCHRG_CS_ConsentType";
constexpr std::string_view documentType = "https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_DocumentType";
constexpr std::string_view flowType = "https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_FlowType";
constexpr std::string_view pzn = "http://fhir.de/CodeSystem/ifa/pzn";
constexpr std::string_view atc = "http://fhir.de/CodeSystem/bfarm/atc";
constexpr std::string_view ask = "http://fhir.de/CodeSystem/ask";
constexpr std::string_view sct = "http://snomed.info/sct";
constexpr std::string_view origin = "https://gematik.de/fhir/directory/CodeSystem/Origin";
} // namespace code_system

namespace structure_definition
{
constexpr std::string_view parameters = "http://hl7.org/fhir/StructureDefinition/Parameters";
constexpr std::string_view create_task_parameters ="https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_PAR_CreateOperation_Input";
constexpr std::string_view activate_task_parameters = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_PAR_ActivateOperation_Input";
constexpr std::string_view subscription = "http://hl7.org/fhir/StructureDefinition/Subscription";
constexpr std::string_view operationoutcome = "http://hl7.org/fhir/StructureDefinition/OperationOutcome";

constexpr std::string_view acceptDate = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_AcceptDate";
constexpr std::string_view auditEvent = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_AuditEvent";
constexpr std::string_view beneficiary = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_Beneficiary";
constexpr std::string_view binary = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Binary";
constexpr std::string_view chargeItem = "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_ChargeItem";
constexpr std::string_view communication = "http://hl7.org/fhir/StructureDefinition/Communication";
constexpr std::string_view communicationChargChangeReply = "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_Communication_ChargChangeReply";
constexpr std::string_view communicationChargChangeReq = "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_Communication_ChargChangeReq";
constexpr std::string_view communicationDispReq = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_DispReq";
constexpr std::string_view communicationInfoReq = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_InfoReq";
constexpr std::string_view communicationLocation = "http://prescriptionserver.telematik/Communication/";
constexpr std::string_view communicationReply = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_Reply";
constexpr std::string_view communicationRepresentative = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_Representative";
constexpr std::string_view composition = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Composition";
constexpr std::string_view consent = "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_PR_Consent";
constexpr std::string_view dataAbsentReason = "http://hl7.org/fhir/StructureDefinition/data-absent-reason";
constexpr std::string_view device = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Device";
constexpr std::string_view digest = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Digest";
constexpr std::string_view dispenseItem = "http://fhir.abda.de/eRezeptAbgabedaten/StructureDefinition/DAV-PKV-PR-ERP-AbgabedatenBundle";
constexpr std::string_view expiryDate = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_ExpiryDate";
constexpr std::string_view kbcExForLegalBasis = "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis";
constexpr std::string_view kbvExErpStatusCoPayment = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_StatusCoPayment";
constexpr std::string_view markingFlag = "https://gematik.de/fhir/erpchrg/StructureDefinition/GEM_ERPCHRG_EX_MarkingFlag";
constexpr std::string_view medication = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Medication";
constexpr std::string_view medicationDispense = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_MedicationDispense";
constexpr std::string_view medicationDispenseDiga = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_MedicationDispense_DiGA";
constexpr std::string_view medicationDispenseBundle = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_CloseOperationInputBundle";
constexpr std::string_view prescriptionID = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_PrescriptionId";
constexpr std::string_view prescriptionType = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_PrescriptionType";
constexpr std::string_view receipt = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Bundle";
constexpr std::string_view task = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Task";

constexpr std::string_view closeOperationInput = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_PAR_CloseOperation_Input";
constexpr std::string_view dispenseOperationInput = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_PAR_DispenseOperation_Input";

constexpr std::string_view lastMedicationDispense = "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_EX_LastMedicationDispense";

constexpr std::string_view prescriptionItem = "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle";
constexpr std::string_view kbv_medication_pzn = "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN";
constexpr std::string_view kbv_composition = "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Composition";
constexpr std::string_view kbv_medication_request = "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Prescription";
constexpr std::string_view kbv_medication_compounding = "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_Compounding";
constexpr std::string_view kbv_medication_free_text = "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_FreeText";
constexpr std::string_view kbv_medication_ingredient = "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_Ingredient";
constexpr std::string_view kbv_practice_supply = "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_PracticeSupply";
constexpr std::string_view kbv_for_coverage = "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Coverage";
constexpr std::string_view kbv_for_organization = "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Organization";
constexpr std::string_view kbv_for_patient = "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Patient";
constexpr std::string_view kbv_for_practitioner = "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Practitioner";
constexpr std::string_view kbv_for_practitioner_role =
    "https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_PractitionerRole";
constexpr std::string_view kbv_pr_evdga_bundle = "https://fhir.kbv.de/StructureDefinition/KBV_PR_EVDGA_Bundle";
constexpr std::string_view kbv_pr_evdga_health_app_request =
    "https://fhir.kbv.de/StructureDefinition/KBV_PR_EVDGA_HealthAppRequest";

constexpr std::string_view epa_op_provide_prescription_erp_input_parameters =
    "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-op-provide-prescription-erp-input-parameters";
constexpr std::string_view epa_op_provide_dispensation_erp_input_parameters =
    "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-op-provide-dispensation-erp-input-parameters";
constexpr std::string_view epa_op_cancel_prescription_erp_input_parameters =
    "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-op-cancel-prescription-erp-input-parameters";
constexpr std::string_view epa_op_rx_prescription_erp_output_parameters =
    "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-op-rx-prescription-erp-output-parameters";
constexpr std::string_view epa_op_rx_dispensation_erp_output_parameters =
    "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-op-rx-dispensation-erp-output-parameters";
constexpr std::string_view organization_directory =
    "https://gematik.de/fhir/directory/StructureDefinition/OrganizationDirectory";
constexpr std::string_view epa_medication_pzn_ingredient =
    "https://gematik.de/fhir/epa-medication/StructureDefinition/epa-medication-pzn-ingredient";
} // namespace structure_definition

namespace operation_definition
{
constexpr std::string_view create = "https://gematik.de/fhir/erp/OperationDefinition/CreateOperationDefinition";
constexpr std::string_view activate = "https://gematik.de/fhir/erp/OperationDefinition/ActivateOperationDefinition";
constexpr std::string_view accept = "https://gematik.de/fhir/erp/OperationDefinition/AcceptOperationDefinition";
constexpr std::string_view reject = "https://gematik.de/fhir/erp/OperationDefinition/RejectOperationDefinition";
constexpr std::string_view dispense = "https://gematik.de/fhir/erp/OperationDefinition/DispenseOperationDefinition";
constexpr std::string_view close = "https://gematik.de/fhir/erp/OperationDefinition/CloseOperationDefinition";
constexpr std::string_view abort = "https://gematik.de/fhir/erp/OperationDefinition/AbortOperationDefinition";
constexpr std::string_view providePrescriptionErpOP =
    "https://gematik.de/fhir/epa-medication/OperationDefinition/provide-prescription-erp-OP";
} // namespace operation_definition

namespace extension
{
constexpr std::string_view alternativeIk = "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK";
} // namespace extension

namespace parameter
{
constexpr std::string_view rxPrescription = "rxPrescription";
constexpr std::string_view rxDispensation = "rxDispensation";
namespace part
{
constexpr std::string_view prescriptionId = "prescriptionId";
constexpr std::string_view authoredOn = "authoredOn";
constexpr std::string_view medicationRequest = "medicationRequest";
constexpr std::string_view medication = "medication";
constexpr std::string_view organization = "organization";
constexpr std::string_view practitioner = "practitioner";
constexpr std::string_view medicationDispense = "medicationDispense";
}
}

namespace elements
{
const std::string separator{"/"};
} // namespace elements

class ElementName
{
public:
    template <typename T>
    static std::string path(const T& nodeName)
    {
        return node(nodeName);
    }
    template <typename First, typename ... Rest>
    static std::string path(const First& first, const Rest& ... rest)
    {
        return path(first) + path(rest ...);
    }
    explicit ElementName(const std::string& name) :
        mName(name)
    {
    }
    [[nodiscard]] operator const std::string& () const
    {
        return mName;
    }
    [[nodiscard]] operator std::string_view () const
    {
        return mName;
    }
private:
    static std::string node(const std::string& node)
    {
        return elements::separator + node;
    }
    template<typename TNumeric>
        requires std::integral<TNumeric>
    static std::string node(TNumeric node)
    {
        return elements::separator + std::to_string(static_cast<size_t>(node));
    }
    const std::string mName;
};

namespace elements
{
const ElementName about{ "about" };
const ElementName actor{ "actor" };
const ElementName address{ "address" };
const ElementName amount{ "amount" };
const ElementName assigner{ "assigner" };
const ElementName author{ "author" };
const ElementName basedOn{ "basedOn" };
const ElementName category{ "category" };
const ElementName code{ "code" };
const ElementName codeCodeableConcept{ "codeCodeableConcept" };
const ElementName coding{ "coding" };
const ElementName contained{ "contained" };
const ElementName contact{ "contact" };
const ElementName contentAttachment{ "contentAttachment" };
const ElementName contentReference{ "contentReference" };
const ElementName contentString{ "contentString" };
const ElementName contentType{ "contentType" };
const ElementName creation{ "creation" };
const ElementName data{ "data" };
const ElementName dateTime{ "dateTime" };
const ElementName denominator{ "denominator" };
const ElementName deviceName{ "deviceName" };
const ElementName display{ "display" };
const ElementName end{ "end" };
const ElementName enterer{ "enterer" };
const ElementName enteredDate{ "enteredDate" };
const ElementName entry{ "entry" };
const ElementName extension{ "extension" };
const ElementName family{ "family" };
const ElementName _family{ "_family" };
const ElementName form{ "form" };
const ElementName hash{ "hash" };
const ElementName id{ "id" };
const ElementName identifier{ "identifier" };
const ElementName ingredient{ "ingredient" };
const ElementName language{ "language" };
const ElementName line{ "line" };
const ElementName _line{ "_line" };
const ElementName medicationReference { "medicationReference" };
const ElementName meta{ "meta" };
const ElementName mode{ "mode" };
const ElementName name{ "name" };
const ElementName numerator{ "numerator" };
const ElementName parameter{ "parameter" };
const ElementName part{ "part" };
const ElementName path{ "path" };
const ElementName patient{ "patient" };
const ElementName payload{ "payload" };
const ElementName payor{ "payor" };
const ElementName performer{ "performer" };
const ElementName period{ "period" };
const ElementName prefix{ "prefix" };
const ElementName _prefix{ "_prefix" };
const ElementName profile{ "profile" };
const ElementName qualification{ "qualification" };
const ElementName received{ "received" };
const ElementName recipient{ "recipient" };
const ElementName reference{ "reference" };
const ElementName resource{ "resource" };
const ElementName resourceType{ "resourceType" };
const ElementName search{ "search" };
const ElementName sender{ "sender" };
const ElementName sent{ "sent" };
const ElementName serialNumber{ "serialNumber" };
const ElementName size{ "size" };
const ElementName start{ "start" };
const ElementName status{ "status" };
const ElementName subject{ "subject" };
const ElementName supportingInformation{ "supportingInformation" };
const ElementName strength{ "strength" };
const ElementName system{ "system" };
const ElementName tag{ "tag" };
const ElementName telecom{ "telecom" };
const ElementName text{ "text" };
const ElementName title{ "title" };
const ElementName type{ "type" };
const ElementName unit{ "unit" };
const ElementName url{ "url" };
const ElementName use{ "use" };
const ElementName value{ "value" };
const ElementName valueBoolean{ "valueBoolean" };
const ElementName valueCode{ "valueCode" };
const ElementName valueCoding{ "valueCoding" };
const ElementName valueDate{ "valueDate" };
const ElementName valuePeriod{ "valuePeriod" };
const ElementName valueRatio{ "valueRatio" };
const ElementName valueIdentifier{ "valueIdentifier" };
const ElementName valueString{ "valueString" };
const ElementName version{ "version" };
const ElementName whenHandedOver{ "whenHandedOver" };
const ElementName whenPrepared{ "whenPrepared" };
const ElementName lastMedicationDispense{ "lastMedicationDispense" };

} // namespace resource

} // namespace elements

} // namespace model

#endif

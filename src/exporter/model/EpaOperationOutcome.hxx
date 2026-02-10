/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPERATIONOUTCOME_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPERATIONOUTCOME_HXX

#include "shared/model/OperationOutcome.hxx"

namespace model
{

class EPAOperationOutcome
{
public:
    static EPAOperationOutcome fromJson(const std::string& json, const JsonValidator& jsonValidator);
    static EPAOperationOutcome fromXml(const std::string& xml, const XmlValidator& validator);
    explicit EPAOperationOutcome(model::OperationOutcome&& operationOutcome);

    // both 1.0.6: https://simplifier.net/packages/de.gematik.epa.medication/1.0.6-2/files/2843697
    // and 1.2.0: https://simplifier.net/packages/de.gematik.epa.medication/1.2.0/files/2975140
    enum class EPAOperationOutcomeCS : uint8_t
    {
        SVC_IDENTITY_MISMATCH,
        MEDICATIONSVC_NO_VALID_STRUCTURE,
        MEDICATIONSVC_PRESCRIPTION_NO_EXIST,
        MEDICATIONSVC_PRESCRIPTION_DUPLICATE,
        MEDICATIONSVC_PRESCRIPTION_STATUS,
        MEDICATIONSVC_DISPENSATION_NO_EXIST,
        MEDICATIONSVC_DISPENSATION_STATUS,
        MEDICATIONSVC_OPERATION_SUCCESS,
        MEDICATIONSVC_PARAMETERS_REFERENCE_NO_EXIST,
        MEDSVC_NO_VALID_STRUCTURE,
        MEDSVC_PRESCRIPTION_NO_EXIST,
        MEDSVC_PRESCRIPTION_DUPLICATE,
        MEDSVC_PRESCRIPTION_STATUS,
        MEDSVC_DISPENSATION_NO_EXIST,
        MEDSVC_DISPENSATION_STATUS,
        MEDSVC_OPERATION_SUCCESS,
        MEDSVC_PARAMETERS_REFERENCE_NO_EXIST,
        MEDSVC_STATUS_INVALID,
        MEDSVC_STATEMENT_NO_EXIST,
        MEDSVC_PARAMETERS_INVALID_CONTENT,
        MEDSVC_DOSAGE_INVALID,
        MEDSVC_EMP_CHRONOLOGY_ID_MISMATCH,
        MEDSVC_OPERATION_OUTSIDE_BATCH,
        MEDSVC_ALREADY_LINKED,
        MEDSVC_EMP_ENTRY_ALREADY_EXISTS,
        MEDSVC_EMP_NO_EXIST,
        GENERIC_OPERATION_OUTCOME_CODE// placeholder for all codes from http://terminology.hl7.org/CodeSystem/operation-outcome
    };

    class Issue : public OperationOutcome::Issue
    {
    public:
        EPAOperationOutcomeCS detailsCode{EPAOperationOutcomeCS::GENERIC_OPERATION_OUTCOME_CODE};
    };

    std::vector<Issue> getIssues() const;

private:
    model::OperationOutcome mOperationOutcome;
};

}// model

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_EPAOPERATIONOUTCOME_HXX

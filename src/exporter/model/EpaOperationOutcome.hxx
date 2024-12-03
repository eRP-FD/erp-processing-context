/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
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

    // https://simplifier.net/epa-medication-v3-0/epa-ms-operation-outcome-codes-cs
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
        MEDICATIONSVC_MEDICATIONPLAN_NO_EXIST,
        MEDICATIONSCV_MEDICINAL_PRODUCT_PACKAGE_NOT_ALLOWED,
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

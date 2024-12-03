/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/EpaOperationOutcome.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/model/Coding.hxx"

namespace model
{

EPAOperationOutcome EPAOperationOutcome::fromJson(const std::string& json, const JsonValidator& jsonValidator)
{
    return EPAOperationOutcome(model::OperationOutcome::fromJson(json, jsonValidator));
}

EPAOperationOutcome EPAOperationOutcome::fromXml(const std::string& xml, const XmlValidator& validator)
{
    return EPAOperationOutcome(model::OperationOutcome::fromXml(xml, validator));
}

EPAOperationOutcome::EPAOperationOutcome(OperationOutcome&& operationOutcome)
    : mOperationOutcome(std::move(operationOutcome))
{
}

std::vector<EPAOperationOutcome::Issue> EPAOperationOutcome::getIssues() const
{
    std::vector<Issue> ret;

    for (const auto& baseIssue : mOperationOutcome.issues())
    {
        ModelExpect(baseIssue.detailsCodings.size() == 1, "issue.details expected to be 1..1");
        ModelExpect(baseIssue.detailsCodings[0].getCode().has_value(), "Coding contains no Code");

        auto detailsCode = magic_enum::enum_cast<EPAOperationOutcomeCS>(*baseIssue.detailsCodings[0].getCode())
                               .value_or(EPAOperationOutcomeCS::GENERIC_OPERATION_OUTCOME_CODE);

        Issue issue{baseIssue, detailsCode};
        ret.emplace_back(issue);
    }

    return ret;
}

}

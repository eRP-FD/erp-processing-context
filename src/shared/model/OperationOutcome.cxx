/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/OperationOutcome.hxx"
#include "shared/model/Coding.hxx"
#include "shared/model/RapidjsonDocument.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/Expect.hxx"

#include <magic_enum/magic_enum.hpp>
#include <algorithm>
#include <mutex>// for call_once


namespace rj = rapidjson;

namespace model
{

namespace
{

constexpr std::string_view operation_outcome_template = R"(
{
  "resourceType": "OperationOutcome",
  "meta": {
    "profile": [
      "http://hl7.org/fhir/StructureDefinition/OperationOutcome"
    ]
  },
  "issue":[
  ]
})";

std::once_flag onceFlag;

struct OperationOutcomeTemplateMark;
RapidjsonNumberAsStringParserDocument<OperationOutcomeTemplateMark> operationOutcomeTemplate;

void initTemplates()
{
    rj::StringStream s1(operation_outcome_template.data());
    operationOutcomeTemplate->ParseStream<rj::kParseNumbersAsStringsFlag, rj::CustomUtf8>(s1);
}

// definition of JSON pointers:
const rj::Pointer issuePointer("/issue");
const rj::Pointer issueSeverityPointer("/severity");
const rj::Pointer issueCodePointer("/code");
const rj::Pointer issueDetailsTextPointer("/details/text");
const rj::Pointer Pointer("/details/text");
const rj::Pointer issueDetailsCodingPointer("/details/coding");
const rj::Pointer issueDiagnosticsPointer("/diagnostics");
const resource::ElementName expressionElemName{"expression"};
const rj::Pointer issueExpressionPointer(resource::ElementName::path(expressionElemName));

}// anonymous namespace


// static
std::string OperationOutcome::Issue::typeToString(OperationOutcome::Issue::Type type)
{
    std::string result = std::string(magic_enum::enum_name(type));
    ModelExpect(! result.empty(), "Invalid issue code type " + std::to_string(static_cast<int>(type)));
    std::replace(result.begin(), result.end(), '_', '-');
    return result;
}

//static
OperationOutcome::Issue::Type OperationOutcome::Issue::stringToType(std::string str)
{
    std::replace(str.begin(), str.end(), '-', '_');
    const auto result = magic_enum::enum_cast<OperationOutcome::Issue::Type>(str);
    ModelExpect(result.has_value(), "Invalid issue type enum value " + str);
    return result.value();
}


OperationOutcome::OperationOutcome(Issue&& primaryIssue)
    : Resource<OperationOutcome>(Resource::NoProfile,
                                 []() {
                                     std::call_once(onceFlag, initTemplates);
                                     return operationOutcomeTemplate;
                                 }()
                                     .instance())
{
    addIssue(std::move(primaryIssue));
}


OperationOutcome::OperationOutcome(NumberAsStringParserDocument&& jsonTree)
    : Resource<OperationOutcome>(std::move(jsonTree))
{
    std::call_once(onceFlag, initTemplates);
}


void OperationOutcome::addIssue(Issue&& issue)
{
    auto entry = createEmptyObject();
    setKeyValue(entry, issueSeverityPointer, magic_enum::enum_name(issue.severity));
    setKeyValue(entry, issueCodePointer, Issue::typeToString(issue.code));
    if (issue.detailsText.has_value() && ! issue.detailsText->empty())
        setKeyValue(entry, issueDetailsTextPointer, issue.detailsText.value());
    if (issue.diagnostics.has_value() && ! issue.diagnostics->empty())
        setKeyValue(entry, issueDiagnosticsPointer, issue.diagnostics.value());

    if (! issue.expression.empty())
    {
        rj::Value expressionArray(rj::kArrayType);
        for (const auto& expr : issue.expression)
            addStringToArray(expressionArray, expr);
        setKeyValue(entry, issueExpressionPointer, expressionArray);
    }

    addToArray(issuePointer, std::move(entry));
}


std::vector<OperationOutcome::Issue> OperationOutcome::issues() const
{
    std::vector<OperationOutcome::Issue> results;
    for (size_t i = 0;; ++i)
    {
        const auto* issue = getMemberInArray(issuePointer, i);
        if (issue == nullptr)
            break;

        const auto issueSeverity = getStringValue(*issue, issueSeverityPointer);
        const auto severityEnum = magic_enum::enum_cast<OperationOutcome::Issue::Severity>(std::string(issueSeverity));
        ModelExpect(severityEnum.has_value(), "Invalid severity enum value " + std::string(issueSeverity));
        const auto issueCode = std::string(getStringValue(*issue, issueCodePointer));
        const auto codeEnum = Issue::stringToType(issueCode);
        const auto issueDetailsText = getOptionalStringValue(*issue, issueDetailsTextPointer);
        const auto issueDiagnostics = getOptionalStringValue(*issue, issueDiagnosticsPointer);

        std::vector<std::string> expressions;
        for (auto j = 0;; ++j)
        {
            const auto issueExpression =
                getOptionalStringValue(*issue, rj::Pointer(resource::ElementName::path(expressionElemName, j)));
            if (! issueExpression.has_value())
                break;
            expressions.emplace_back(issueExpression.value());
        }

        std::vector<Coding> detailsCodes;
        const auto* codingArray = issueDetailsCodingPointer.Get(*issue);
        if (codingArray)
        {
            ModelExpect(codingArray->IsArray(), "issue.details.coding is expected to be an array");
            for (const auto& coding : codingArray->GetArray())
            {
                detailsCodes.emplace_back(coding);
            }
        }

        results.emplace_back(OperationOutcome::Issue{
            severityEnum.value(), codeEnum, std::optional<std::string>(issueDetailsText), std::move(detailsCodes),
            std::optional<std::string>(issueDiagnostics), expressions});
    }

    return results;
}

std::string OperationOutcome::concatDetails() const
{
    std::string out;
    std::string sep;
    for (const auto& issue : issues())
    {
        if (issue.detailsText)
        {
            out += sep + *issue.detailsText;
            sep = ", ";
        }
    }
    return out;
}

// This fixed mapping from HTTP code to issue code might be to unexact.
// It is a first approach for an easy creation of an error response.
OperationOutcome::Issue::Type OperationOutcome::httpCodeToOutcomeIssueType(HttpStatus httpCode)
{
    switch (httpCode)
    {
        case HttpStatus::BadRequest:
            return Issue::Type::invalid;
        case HttpStatus::Unauthorized:
            return Issue::Type::unknown;
        case HttpStatus::Forbidden:
            return Issue::Type::forbidden;
        case HttpStatus::NotFound:
            return Issue::Type::not_found;
        case HttpStatus::MethodNotAllowed:
            return Issue::Type::not_supported;
        case HttpStatus::Conflict:
            return Issue::Type::conflict;
        case HttpStatus::Gone:
            return Issue::Type::processing;
        case HttpStatus::UnsupportedMediaType:
            return Issue::Type::value;
        case HttpStatus::TooManyRequests:
            return Issue::Type::transient;
        default:
            return Issue::Type::processing;
    }
}


bool operator==(const OperationOutcome::Issue& lhs, const OperationOutcome::Issue& rhs)
{
    return lhs.severity == rhs.severity && lhs.code == rhs.code && lhs.detailsText == rhs.detailsText &&
           lhs.diagnostics == rhs.diagnostics && lhs.expression == rhs.expression;
}

std::optional<model::Timestamp> OperationOutcome::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}


}// namespace model

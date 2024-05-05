/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/OperationOutcome.hxx"
#include "test/util/StaticData.hxx"

#include <magic_enum/magic_enum.hpp>

#include <gtest/gtest.h>

using namespace model;


namespace
{

void checkOperationOutcome(const model::OperationOutcome& operationOutcome)//NOLINT(readability-function-cognitive-complexity)
{
    const auto issues = operationOutcome.issues();
    EXPECT_EQ(issues.size(), 4);
    {
        const auto& issue = issues[0];
        EXPECT_EQ(issue.severity, OperationOutcome::Issue::Severity::error);
        EXPECT_EQ(issue.code, OperationOutcome::Issue::Type::code_invalid);
        EXPECT_EQ(issue.detailsText, "The code 'W' is not known and not legal in this context");
        EXPECT_EQ(issue.diagnostics, "FHIRProcessors.Patient.processGender line 2453");
        EXPECT_EQ(issue.expression, std::vector({std::string("Patient.gender")}));
    }
    {
        const auto& issue = issues[1];
        EXPECT_EQ(issue.severity, OperationOutcome::Issue::Severity::warning);
        EXPECT_EQ(issue.code, OperationOutcome::Issue::Type::multiple_matches);
        EXPECT_EQ(issue.detailsText, "Something looks strange");
        EXPECT_EQ(issue.diagnostics, "Processor.cxx, line 1000");
        EXPECT_EQ(issue.expression, std::vector({std::string("Some-text-1"), std::string("Some-text-2")}));
    }
    {
        const auto& issue = issues[2];
        EXPECT_EQ(issue.severity, OperationOutcome::Issue::Severity::information);
        EXPECT_EQ(issue.code, OperationOutcome::Issue::Type::informational);
        EXPECT_EQ(issue.detailsText, "Everything okay");
        EXPECT_FALSE(issue.diagnostics.has_value());
        EXPECT_TRUE(issue.expression.empty());
    }
    {
        const auto& issue = issues[3];
        EXPECT_EQ(issue.severity, OperationOutcome::Issue::Severity::fatal);
        EXPECT_EQ(issue.code, OperationOutcome::Issue::Type::exception);
        EXPECT_FALSE(issue.detailsText.has_value());
        EXPECT_FALSE(issue.diagnostics.has_value());
        EXPECT_TRUE(issue.expression.empty());
    }
}

}


TEST(OperationOutcomeTest, Construct)//NOLINT(readability-function-cognitive-complexity)
{
    const OperationOutcome::Issue issueCompare1 {
        OperationOutcome::Issue::Severity::error,
        OperationOutcome::Issue::Type::code_invalid,
        "details-1", "diagnostics-1",
        { "expr-1-1", "expr-1-2", "expr-1-3", "expr-1-4" } };
    auto issue1 =  issueCompare1;
    OperationOutcome operationOutcome(std::move(issue1));

    const OperationOutcome::Issue issueCompare2 {
        OperationOutcome::Issue::Severity::fatal,
        OperationOutcome::Issue::Type::exception,
        {}, "diagnostics-2",
        {} };
    auto issue2 = issueCompare2;
    operationOutcome.addIssue(std::move(issue2));

    const OperationOutcome::Issue issueCompare3 {
        OperationOutcome::Issue::Severity::warning,
        OperationOutcome::Issue::Type::not_found,
        "details-3", {},
        { "expr-3-1" } };
    auto issue3 = issueCompare3;
    operationOutcome.addIssue(std::move(issue3));

    const OperationOutcome::Issue issueCompare4 {
        OperationOutcome::Issue::Severity::information,
        OperationOutcome::Issue::Type::informational,
        "details-4", {}, {} };
    auto issue4 = issueCompare4;
    operationOutcome.addIssue(std::move(issue4));

    EXPECT_NO_THROW((void)operationOutcome.serializeToXmlString());
    EXPECT_NO_THROW((void)operationOutcome.serializeToJsonString());

    const auto issues = operationOutcome.issues();
    ASSERT_EQ(issues.size(), 4);
    EXPECT_EQ(issueCompare1, issues[0]);
    EXPECT_EQ(issueCompare2, issues[1]);
    EXPECT_EQ(issueCompare3, issues[2]);
    EXPECT_EQ(issueCompare4, issues[3]);
}


TEST(OperationOutcomeTest, ConstructFromJson)
{
const std::string json = R"(
{
  "resourceType": "OperationOutcome",
  "meta": {
    "profile": [
      "http://hl7.org/fhir/StructureDefinition/OperationOutcome"
    ]
  },
  "issue": [
    {
      "severity": "error",
      "code": "code-invalid",
      "details": {
        "text": "The code 'W' is not known and not legal in this context"
      },
      "diagnostics": "FHIRProcessors.Patient.processGender line 2453",
      "expression": [
        "Patient.gender"
      ]
    },
    {
      "severity": "warning",
      "code": "multiple-matches",
      "details": {
        "text": "Something looks strange"
      },
      "diagnostics": "Processor.cxx, line 1000",
      "expression": [
        "Some-text-1",
        "Some-text-2"
      ]
    },
    {
      "severity": "information",
      "code": "informational",
      "details": {
        "text": "Everything okay"
      }
    },
    {
      "severity": "fatal",
      "code": "exception"
    }
  ]
})";

    ASSERT_NO_FATAL_FAILURE(checkOperationOutcome(OperationOutcome::fromJsonNoValidation(json)));
}


TEST(OperationOutcomeTest, ConstructFromXml)
{
    const std::string xml = R"(
<OperationOutcome xmlns="http://hl7.org/fhir">
   <meta>
      <profile value="http://hl7.org/fhir/StructureDefinition/OperationOutcome" />
   </meta>
   <issue>
      <severity value="error" />
      <code value="code-invalid" />
      <details>
         <text value="The code 'W' is not known and not legal in this context" />
      </details>
      <diagnostics value="FHIRProcessors.Patient.processGender line 2453" />
      <expression value="Patient.gender" />
   </issue>
   <issue>
      <severity value="warning" />
      <code value="multiple-matches" />
      <details>
         <text value="Something looks strange" />
      </details>
      <diagnostics value="Processor.cxx, line 1000" />
      <expression value="Some-text-1" />
      <expression value="Some-text-2" />
   </issue>
   <issue>
      <severity value="information" />
      <code value="informational" />
      <details>
         <text value="Everything okay" />
      </details>
   </issue>
   <issue>
      <severity value="fatal" />
      <code value="exception" />
   </issue>
</OperationOutcome>
)";

    ASSERT_NO_FATAL_FAILURE(checkOperationOutcome(OperationOutcome::fromXml(
        xml, *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(), SchemaType::fhir,
        model::ResourceVersion::supportedBundles())));
}


TEST(OperationOutcomeTest, IssueCodeType)//NOLINT(readability-function-cognitive-complexity)
{
    const auto codeTypeValues = {
        "invalid", "structure", "required", "value", "invariant", "security", "login", "unknown", "expired", "forbidden",
        "suppressed", "processing", "not-supported", "duplicate", "multiple-matches", "not-found", "deleted", "too-long",
        "code-invalid", "extension", "too-costly", "business-rule", "conflict", "transient", "lock-error", "no-store",
        "exception", "timeout", "incomplete", "throttled", "informational" };
    for(const auto& codeTypeValue : codeTypeValues)
    {
        OperationOutcome::Issue::Type issueCode{};
        ASSERT_NO_THROW(issueCode = OperationOutcome::Issue::stringToType(codeTypeValue));
        std::string issueCodeStr;
        ASSERT_NO_THROW(issueCodeStr = OperationOutcome::Issue::typeToString(issueCode));
        EXPECT_EQ(issueCodeStr, codeTypeValue);
    }

    EXPECT_THROW((void)OperationOutcome::Issue::stringToType("invalid-value"), ModelException);
    EXPECT_THROW((void)OperationOutcome::Issue::typeToString(static_cast<OperationOutcome::Issue::Type>(9999)), ModelException);
}

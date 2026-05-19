/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/dosagetext/Validator.hxx"
#include "shared/util/dosagetext/Generator.hxx"
#include "shared/util/dosagetext/Language.hxx"
#include "shared/util/dosagetext/ScriptVersion.hxx"
#include "shared/model/extensions/GeneratedDosageInstructionsMeta.hxx"

namespace dosagetext
{
void Validator::validate(const std::vector<model::DosageDgMP>& dosages,
                         const model::UnspecifiedExtension& renderedDosageExtension,
                         const model::GeneratedDosageInstructionsMeta& meta)
{
    using namespace std::string_literals;
    A_28568.start("Validierung von Version und Sprache");
    const auto& givenVersionStr = meta.algorithmVersion();
    const auto givenVersion = findVersion(givenVersionStr);
    ErpExpectWithDiagnostics(givenVersion.has_value(), HttpStatus::BadRequest,
                             "Validation of rendered dosage-instructions: The algorithm version "s +
                                 std::string{givenVersionStr} + " is unknown",
                             "known versions are: " + knownVersionsList());
    const auto givenLanguageStr = meta.language();
    const auto givenLanguage = findLanguage(givenLanguageStr);
    ErpExpectWithDiagnostics(givenLanguage.has_value(), HttpStatus::BadRequest,
                             "Validation of rendered dosage-instructions: The language "s +
                                 std::string{givenLanguageStr} + " is unknown",
                             "known languages are: " + knownLanguagesList());
    A_28568.finish();

    A_28569.start("Validierung der generierten Dosierungsangabe");
    const Generator generator{*givenVersion, *givenLanguage};
    auto renderedDosageInstruction = generator.render(dosages);
    const auto givenRenderedDosageInstruction = renderedDosageExtension.valueMarkdown();
    ErpExpectWithDiagnostics(givenRenderedDosageInstruction.has_value(), HttpStatus::BadRequest,
                             "Validation of rendered dosage-instructions: valueMarkdown is missing in extension " +
                                 std::string{renderedDosageExtension.url()},
                             renderedDosageInstruction.renderedDosageInstruction);
    ErpExpectWithDiagnostics(
        *givenRenderedDosageInstruction == renderedDosageInstruction.renderedDosageInstruction, HttpStatus::BadRequest,
        "Validation of rendered dosage-instructions: valueMarkdown does not match the expected string in extension " +
            std::string{renderedDosageExtension.url()},
        "expected: " + renderedDosageInstruction.renderedDosageInstruction);
    A_28569.finish();
}
}

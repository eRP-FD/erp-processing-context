/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/validator/FhirPathValidator.hxx"
#include "test/fhirtools/SampleValidation.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <test/fhirtools/DefaultFhirStructureRepository.hxx>

class Erp12159ReferenceTypeCheckTest : public fhirtools::test::SampleValidationTest<Erp12159ReferenceTypeCheckTest>
{
public:
    static const fhirtools::FhirStructureRepository* makeRepo()
    {
        return std::addressof(DefaultFhirStructureRepository::get());
    }
};

TEST_P(Erp12159ReferenceTypeCheckTest, run)
{
    ASSERT_NO_FATAL_FAILURE(run());
}

using Sample = fhirtools::test::Sample;

// clang-format off
INSTANTIATE_TEST_SUITE_P(sample, Erp12159ReferenceTypeCheckTest, ::testing::Values(
                        Sample{
                            "AuditEvent", "test/fhir-path/issues/ERP-12159-RefereceTypeCheck/InvalidUrl.xml",
                            {
                                {
                                    std::make_tuple(fhirtools::Severity::error, "Unknown type: http://example.org/fhir/StructureDefinition/Device"),
                                    "AuditEvent.source.observer.type"
                                }
                            },
                            {
                                {"dom-6", "AuditEvent"}
                            }
                        },
                        Sample{
                            "AuditEvent", "test/fhir-path/issues/ERP-12159-RefereceTypeCheck/UrlInsteadOfType.xml",
                            {
                                {
                                    std::make_tuple(fhirtools::Severity::error, "Urls only allowed for Logical Models."),
                                    "AuditEvent.source.observer.type"
                                }
                            },
                            {
                                {"dom-6", "AuditEvent"}
                            }
                        },
                        Sample{
                            "AuditEvent", "test/fhir-path/issues/ERP-12159-RefereceTypeCheck/InvalidType.xml",
                            {

                                {
                                    std::make_tuple(fhirtools::Severity::error,
                                        R"(Non of the allowed Target Profiles [)"
                                        R"("http://hl7.org/fhir/StructureDefinition/Device", )"
                                        R"("http://hl7.org/fhir/StructureDefinition/Organization", )"
                                        R"("http://hl7.org/fhir/StructureDefinition/Patient", )"
                                        R"("http://hl7.org/fhir/StructureDefinition/Practitioner", )"
                                        R"("http://hl7.org/fhir/StructureDefinition/PractitionerRole", )"
                                        R"("http://hl7.org/fhir/StructureDefinition/RelatedPerson"] )"
                                        R"(matches type: http://hl7.org/fhir/StructureDefinition/Medication|4.0.1)"),
                                    "AuditEvent.source.observer.type"
                                }
                            },
                            {
                                {"dom-6", "AuditEvent"}
                            }
                        },
                        Sample{
                            "AuditEvent", "test/fhir-path/issues/ERP-12159-RefereceTypeCheck/UrlOfWrongType.xml",
                            {
                                {
                                    std::make_tuple(fhirtools::Severity::error, "Urls only allowed for Logical Models."),
                                    "AuditEvent.source.observer.type"
                                },
                                {
                                    std::make_tuple(fhirtools::Severity::error,
                                        R"(Non of the allowed Target Profiles [)"
                                        R"("http://hl7.org/fhir/StructureDefinition/Device", )"
                                        R"("http://hl7.org/fhir/StructureDefinition/Organization", )"
                                        R"("http://hl7.org/fhir/StructureDefinition/Patient", )"
                                        R"("http://hl7.org/fhir/StructureDefinition/Practitioner", )"
                                        R"("http://hl7.org/fhir/StructureDefinition/PractitionerRole", )"
                                        R"("http://hl7.org/fhir/StructureDefinition/RelatedPerson"] )"
                                        R"(matches type: http://hl7.org/fhir/StructureDefinition/Group|4.0.1)"),
                                    "AuditEvent.source.observer.type"
                                }
                            },
                            {
                                {"dom-6", "AuditEvent"}
                            }
                        }
), &Sample::name);
// clang-format on

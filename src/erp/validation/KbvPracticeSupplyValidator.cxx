/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/validation/KbvPracticeSupplyValidator.hxx"
#include "erp/model/Extension.hxx"
#include "erp/model/KbvPracticeSupply.hxx"
#include "erp/util/Expect.hxx"
#include "erp/validation/KbvValidationUtils.hxx"


void KbvPracticeSupplyValidator::validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                                          const InCodeValidator& inCodeValidator) const
{
    doValidate(dynamic_cast<const model::KbvPracticeSupply&>(resource), xmlValidator, inCodeValidator);
}
void KbvPracticeSupplyValidator_V1_0_2::doValidate(const model::KbvPracticeSupply& kbvPracticeSupply,
                                                   const XmlValidator&, const InCodeValidator&) const
{
    const auto& payorExtension = kbvPracticeSupply.getExtension<model::Extension>(
        "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_PracticeSupply_Payor");
    ErpExpect(payorExtension, HttpStatus::BadRequest, "missing extension: KBV_EX_ERP_PracticeSupply_Payor");
    KbvValidationUtils::checkKbvExtensionValueIdentifier("ik", *payorExtension, false,
                                                         "http://fhir.de/NamingSystem/arge-ik/iknr", 9);
    KbvValidationUtils::checkKbvExtensionValueString("name", *payorExtension, true);
    KbvValidationUtils::checkKbvExtensionValueCoding("kostentraegertyp", *payorExtension, true,
                                                     {"http://fhir.de/CodeSystem/versicherungsart-de-basis",
                                                      "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Payor_Type_KBV"},
                                                     {"GKV", "PKV", "PG", "SEL", "SKT", "UK"});
}

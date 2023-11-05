/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvMedicationBase.hxx"
#include "erp/model/KbvMedicationCompounding.hxx"
#include "erp/model/KbvMedicationDummy.hxx"
#include "erp/model/KbvMedicationFreeText.hxx"
#include "erp/model/KbvMedicationIngredient.hxx"
#include "erp/model/KbvMedicationPzn.hxx"
#include "erp/model/ResourceFactory.hxx"
#include "erp/util/String.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"

using namespace model;
template<typename MedicationModelT>
void KbvMedicationGeneric::validateMedication(
    NumberAsStringParserDocument&& medicationDoc, const XmlValidator& xmlValidator,
    const InCodeValidator& inCodeValidator,
    const std::set<model::ResourceVersion::FhirProfileBundleVersion>& supportedBundles)
{
    auto medicationFactory = ResourceFactory<MedicationModelT>::fromJson(std::move(medicationDoc));
    (void) std::move(medicationFactory)
        .getValidated(MedicationModelT::schemaType, xmlValidator, inCodeValidator, supportedBundles);
}

void KbvMedicationGeneric::validateMedication(
    const ErpElement& medicationElement, const XmlValidator& xmlValidator, const InCodeValidator& inCodeValidator,
    const std::set<model::ResourceVersion::FhirProfileBundleVersion>& supportedBundles, bool allowDummyValidation)
{
    using namespace std::string_literals;
    model::NumberAsStringParserDocument medicationDoc;
    std::optional<SchemaType> schemaType;
    const auto& profiles = medicationElement.profiles();
    for (const auto& profStr : medicationElement.profiles())
    {
        const auto* info = model::ResourceVersion::profileInfoFromProfileName(profStr);
        if (info)
        {
            if (info->bundleVersion == model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01 &&
                allowDummyValidation)
            {
                schemaType = model::KbvMedicationDummy::schemaType;
            }
            else
            {
                schemaType = info->schemaType;
            }
            break;
        }
    }
    ErpExpectWithDiagnostics(schemaType.has_value(), HttpStatus::BadRequest,
                             "Could not determine schema type of medication.",
                             "Profiles are: " + String::join(profiles));

    medicationDoc.CopyFrom(*medicationElement.erpValue(), medicationDoc.GetAllocator());
    switch (*schemaType)
    {
        case model::KbvMedicationCompounding::schemaType:
            validateMedication<model::KbvMedicationCompounding>(std::move(medicationDoc), xmlValidator, inCodeValidator,
                                                                supportedBundles);
            break;
        case model::KbvMedicationDummy::schemaType:
            validateMedication<model::KbvMedicationDummy>(std::move(medicationDoc), xmlValidator, inCodeValidator,
                                                          supportedBundles);
            break;
        case model::KbvMedicationFreeText::schemaType:
            validateMedication<model::KbvMedicationFreeText>(std::move(medicationDoc), xmlValidator, inCodeValidator,
                                                             supportedBundles);
            break;
        case model::KbvMedicationIngredient::schemaType:
            validateMedication<model::KbvMedicationIngredient>(std::move(medicationDoc), xmlValidator, inCodeValidator, supportedBundles);
            break;
        case model::KbvMedicationPzn::schemaType:
            validateMedication<model::KbvMedicationPzn>(std::move(medicationDoc), xmlValidator, inCodeValidator,
                                                        supportedBundles);
            break;
        default:
            ErpFailWithDiagnostics(HttpStatus::BadRequest, "Invalid schema type for medication.",
                                   "Profiles are: " + String::join(profiles));
    }
}

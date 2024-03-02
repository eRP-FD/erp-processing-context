/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/validation/KbvBundleValidator.hxx"
#include "erp/model/Composition.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/KbvComposition.hxx"
#include "erp/model/KbvCoverage.hxx"
#include "erp/model/KbvMedicationCompounding.hxx"
#include "erp/model/KbvMedicationFreeText.hxx"
#include "erp/model/KbvMedicationIngredient.hxx"
#include "erp/model/KbvMedicationPzn.hxx"
#include "erp/model/KbvMedicationBase.hxx"
#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/model/KbvOrganization.hxx"
#include "erp/model/KbvPracticeSupply.hxx"
#include "erp/model/KbvPractitioner.hxx"
#include "erp/model/Patient.hxx"
#include "erp/model/extensions/KBVEXFORLegalBasis.hxx"
#include "erp/validation/KbvValidationUtils.hxx"

#include <date/date.h>

namespace
{

enum class KbvBundleConstraint
{
    erp_compositionPflicht,
    erp_angabePruefnummer,
    erp_angabeZuzahlung,
    erp_angabePLZ,
    erp_angabeNrAusstellendePerson,
    erp_angabeBestriebsstaettennr,
    erp_angabeRechtsgrundlage,
    erp_versionComposition,
    IK_Kostentraeger_BG_UK_Pflicht
};

std::string constraintMessage(KbvBundleConstraint constraint)
{
    switch (constraint)
    {
        case KbvBundleConstraint::erp_compositionPflicht:
            return "Die Ressource vom Typ Composition muss genau einmal vorhanden sein";
        case KbvBundleConstraint::erp_angabePruefnummer:
            return "Prüfnummer nicht vorhanden, aber Pflicht bei den Kostenträger der Typen \"GKV\", \"BG\", \"SKT\" "
                   "oder \"UK\"";
        case KbvBundleConstraint::erp_angabeZuzahlung:
            return "Zuzahlungsstatus nicht vorhanden, aber Pflicht bei den Kostenträgern der Typen \"GKV\", \"BG\", "
                   "\"SKT\" oder \"UK\"";
        case KbvBundleConstraint::erp_angabePLZ:
            return "Postleitzahl nicht vorhanden, aber Pflicht bei den Kostentraegern der Typen \"GKV\", \"BG\", "
                   "\"SKT\" oder \"UK\"";
        case KbvBundleConstraint::erp_angabeNrAusstellendePerson:
            return "Nummer der ausstellenden Person nicht vorhanden, aber Pflicht, wenn es sich um einen Arzt oder "
                   "Zahnarzt handelt";
        case KbvBundleConstraint::erp_angabeBestriebsstaettennr:
            return "Betriebsstaettennummer nicht vorhanden, aber Pflicht, wenn es sich um einen Arzt, Zahnarzt oder "
                   "Arzt in Weiterbildung handelt";
        case KbvBundleConstraint::erp_angabeRechtsgrundlage:
            return "Rechtsgrundlage nicht vorhanden, aber Pflicht bei den Kostentraegern der Typen \"GKV\", \"BG\", "
                   "\"SKT\" oder \"UK\"";
        case KbvBundleConstraint::erp_versionComposition:
            return "Die Instanz der Composition muss vom Profil KBV_PR_ERP_Composition|1.0.1 sein";
        case KbvBundleConstraint::IK_Kostentraeger_BG_UK_Pflicht:
            return "BG-Pflicht";
    }
    Fail("invalid KbvBundleConstraint " + std::to_string(static_cast<std::uintmax_t>(constraint)));
}

std::string constraintError(KbvBundleConstraint constraint, const std::optional<std::string>& additional = std::nullopt)
{
    return KbvValidationUtils::constraintError(constraintMessage(constraint), additional);
}
}

void KbvBundleValidator_V1_0_2::validateEntries(const model::KbvBundle& kbvBundle, const XmlValidator& xmlValidator,
                                                const InCodeValidator& inCodeValidator) const
{
    try
    {
        validateMedicationProfiles(kbvBundle, xmlValidator, inCodeValidator);

        validateEntry<model::KbvCoverage>(kbvBundle, xmlValidator, inCodeValidator, SchemaType::KBV_PR_FOR_Coverage);
        validateEntry<model::KbvComposition>(kbvBundle, xmlValidator, inCodeValidator,
                                             SchemaType::KBV_PR_ERP_Composition);
        validateEntry<model::KbvMedicationRequest>(kbvBundle, xmlValidator, inCodeValidator,
                                                   SchemaType::KBV_PR_ERP_Prescription);
        validateEntry<model::KbvOrganization>(kbvBundle, xmlValidator, inCodeValidator,
                                              SchemaType::KBV_PR_FOR_Organization);
        validateEntry<model::Patient>(kbvBundle, xmlValidator, inCodeValidator, SchemaType::KBV_PR_FOR_Patient);
        validateEntry<model::KbvPractitioner>(kbvBundle, xmlValidator, inCodeValidator,
                                              SchemaType::KBV_PR_FOR_Practitioner);
        validateEntry<model::KbvPracticeSupply>(kbvBundle, xmlValidator, inCodeValidator,
                                                SchemaType::KBV_PR_ERP_PracticeSupply);
    }
    catch (const model::ModelException& ex)
    {
        ErpFail(HttpStatus::BadRequest, ex.what());
    }
}

void KbvBundleValidator_V1_0_2::erp_compositionPflicht(const model::KbvBundle& kbvBundle) const
{
    // -erp-compositionPflicht
    // entry.where(resource is Composition).count()=1
    try
    {
        const auto& compositionBundle = kbvBundle.getResourcesByType<model::Composition>();
        ErpExpect(compositionBundle.size() == 1, HttpStatus::BadRequest,
                  constraintError(KbvBundleConstraint::erp_compositionPflicht));
    }
    catch (const model::ModelException& ex)
    {
        ErpFail(HttpStatus::BadRequest, constraintError(KbvBundleConstraint::erp_compositionPflicht, ex.what()));
    }
}


void KbvBundleValidator_V1_0_2::erp_angabePruefnummer(const model::KbvBundle& kbvBundle) const
{
    // -erp-angabePruefnummer
    // (entry.where(resource is Coverage).exists()
    // and (entry.where(resource is Coverage).resource.type.coding.code='GKV'
    //      or entry.where(resource is Coverage).resource.type.coding.code='BG'
    //      or entry.where(resource is Coverage).resource.type.coding.code='SKT'
    //      or entry.where(resource is Coverage).resource.type.coding.code='UK'))
    // implies entry.where(resource is Composition).resource.author.identifier
    //  .where(system='https://fhir.kbv.de/NamingSystem/KBV_NS_FOR_Pruefnummer').exists()
    try
    {
        if (isKostentraeger(kbvBundle))
        {
            const auto& compositionBundle = kbvBundle.getResourcesByType<model::Composition>();
            ErpExpect(compositionBundle.size() == 1, HttpStatus::BadRequest,
                      constraintError(KbvBundleConstraint::erp_compositionPflicht));
            const auto& authorSystem1 = compositionBundle[0].authorIdentifierSystem(0);
            const auto& authorSystem2 = compositionBundle[0].authorIdentifierSystem(1);
            ErpExpect(authorSystem1 == "https://fhir.kbv.de/NamingSystem/KBV_NS_FOR_Pruefnummer" ||
                          authorSystem2 == "https://fhir.kbv.de/NamingSystem/KBV_NS_FOR_Pruefnummer",
                      HttpStatus::BadRequest, constraintError(KbvBundleConstraint::erp_angabePruefnummer));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFail(HttpStatus::BadRequest, constraintError(KbvBundleConstraint::erp_angabePruefnummer, ex.what()));
    }
}

void KbvBundleValidator_V1_0_2::erp_angabeZuzahlung(const model::KbvBundle& kbvBundle) const
{
    // -erp-angabeZuzahlung
    // (entry.where(resource is MedicationRequest).exists()
    // and entry.where(resource is Coverage).exists()
    // and (entry.where(resource is Coverage).resource.type.coding.code='GKV'
    //     or entry.where(resource is Coverage).resource.type.coding.code='BG'
    //     or entry.where(resource is Coverage).resource.type.coding.code='SKT'
    //     or entry.where(resource is Coverage).resource.type.coding.code='UK'))
    // implies entry.where(resource is MedicationRequest).resource
    //  .extension('https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_StatusCoPayment').exists()
    try
    {
        const auto& medicationRequestBundle = kbvBundle.getResourcesByType<model::KbvMedicationRequest>();
        if (! medicationRequestBundle.empty() && isKostentraeger(kbvBundle))
        {
            for (const auto& medicationRequest : medicationRequestBundle)
            {
                const auto& statusCoPaymentExtension = medicationRequest.statusCoPaymentExtension();
                ErpExpect(statusCoPaymentExtension.has_value(), HttpStatus::BadRequest,
                          constraintError(KbvBundleConstraint::erp_angabeZuzahlung));
            }
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFail(HttpStatus::BadRequest, constraintError(KbvBundleConstraint::erp_angabeZuzahlung, ex.what()));
    }
}


void KbvBundleValidator_V1_0_2::erp_angabePLZ(const model::KbvBundle& kbvBundle) const
{
    // -erp-angabePLZ
    // (entry.where(resource is Patient).exists()
    // and entry.where(resource is Coverage).exists()
    // and (entry.where(resource is Coverage).resource.type.coding.code='GKV'
    //      or entry.where(resource is Coverage).resource.type.coding.code='BG'
    //      or entry.where(resource is Coverage).resource.type.coding.code='SKT'
    //      or entry.where(resource is Coverage).resource.type.coding.code='UK'))
    // implies entry.where(resource is Patient).resource.address.postalCode.exists()
    try
    {
        const auto& patientBundle = kbvBundle.getResourcesByType<model::Patient>();
        if (! patientBundle.empty() && isKostentraeger(kbvBundle))
        {
            for (const auto& patient : patientBundle)
            {
                ErpExpect(patient.postalCode().has_value(), HttpStatus::BadRequest,
                          constraintError(KbvBundleConstraint::erp_angabePLZ));
            }
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFail(HttpStatus::BadRequest, constraintError(KbvBundleConstraint::erp_angabePLZ, ex.what()));
    }
}


void KbvBundleValidator_V1_0_2::erp_angabeNrAusstellendePerson(const model::KbvBundle& kbvBundle) const
{
    // -erp-angabeNrAusstellendePerson
    // (entry.where(resource is Coverage).exists()
    // and (entry.where(resource is Coverage).resource.type.coding.code='GKV'
    //      or entry.where(resource is Coverage).resource.type.coding.code='BG'
    //      or entry.where(resource is Coverage).resource.type.coding.code='SKT'
    //      or entry.where(resource is Coverage).resource.type.coding.code='UK')
    // and (entry.where(resource is Practitioner).resource.qualification.coding.code='00'
    //      or entry.where(resource is Practitioner).resource.qualification.coding.code='01'))
    // implies entry.where(resource is Practitioner).resource.identifier.exists()
    try
    {
        if (isKostentraeger(kbvBundle))
        {
            const auto& practitionerBundle = kbvBundle.getResourcesByType<model::KbvPractitioner>();
            for (const auto& practitioner : practitionerBundle)
            {
                const auto& qualificationTypeCode = practitioner.qualificationTypeCode();
                if (qualificationTypeCode == "00" || qualificationTypeCode == "01")
                {
                    ErpExpect(practitioner.identifierValue().has_value(), HttpStatus::BadRequest,
                              constraintError(KbvBundleConstraint::erp_angabeNrAusstellendePerson));
                }
            }
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFail(HttpStatus::BadRequest,
                constraintError(KbvBundleConstraint::erp_angabeNrAusstellendePerson, ex.what()));
    }
}


void KbvBundleValidator_V1_0_2::erp_angabeBestriebsstaettennr(const model::KbvBundle& kbvBundle) const
{
    // -erp-angabeBestriebsstaettennr
    // (entry.where(resource is Coverage).exists()
    // and (entry.where(resource is Coverage).resource.type.coding.code='GKV'
    //      or entry.where(resource is Coverage).resource.type.coding.code='BG'
    //      or entry.where(resource is Coverage).resource.type.coding.code='SKT'
    //      or entry.where(resource is Coverage).resource.type.coding.code='UK')
    // and (entry.where(resource is Practitioner).resource.qualification.coding.code='00'
    //      or entry.where(resource is Practitioner).resource.qualification.coding.code='01'
    //      or entry.where(resource is Practitioner).resource.qualification.coding.code='03'))
    // implies entry.where(resource is Organization).resource.identifier.exists()
    try
    {
        if (isKostentraeger(kbvBundle))
        {
            const auto& practitionerBundle = kbvBundle.getResourcesByType<model::KbvPractitioner>();
            bool mandatoryOrganizationIdentifier = false;
            for (const auto& practitioner : practitionerBundle)
            {
                const auto& qualificationTypeCode = practitioner.qualificationTypeCode();
                if (qualificationTypeCode == "00" || qualificationTypeCode == "01" || qualificationTypeCode == "03")
                {
                    mandatoryOrganizationIdentifier = true;
                    break;
                }
            }
            if (mandatoryOrganizationIdentifier)
            {
                const auto& organizationBundle = kbvBundle.getResourcesByType<model::KbvOrganization>();
                ErpExpect(! organizationBundle.empty(), HttpStatus::BadRequest,
                          constraintError(KbvBundleConstraint::erp_angabeBestriebsstaettennr));
                for (const auto& organization : organizationBundle)
                {
                    ErpExpect(organization.identifierExists(), HttpStatus::BadRequest,
                              constraintError(KbvBundleConstraint::erp_angabeBestriebsstaettennr));
                }
            }
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFail(HttpStatus::BadRequest, constraintError(KbvBundleConstraint::erp_angabeBestriebsstaettennr, ex.what()));
    }
}


void KbvBundleValidator_V1_0_2::erp_angabeRechtsgrundlage(const model::KbvBundle& kbvBundle) const
{
    // -erp-angabeRechtsgrundlage
    // (entry.where(resource is MedicationRequest).exists()
    // and entry.where(resource is Coverage).exists()
    // and (entry.where(resource is Coverage).resource.type.coding.code='GKV'
    //      or entry.where(resource is Coverage).resource.type.coding.code='BG'
    //      or entry.where(resource is Coverage).resource.type.coding.code='SKT'
    //      or entry.where(resource is Coverage).resource.type.coding.code='UK'))
    // implies entry.where(resource is Composition).resource
    //  .extension('https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis').exists()
    try
    {
        const auto& medicationRequestBundle = kbvBundle.getResourcesByType<model::KbvMedicationRequest>();
        if (! medicationRequestBundle.empty() && isKostentraeger(kbvBundle))
        {
            const auto& compositionBundle = kbvBundle.getResourcesByType<model::Composition>();
            ErpExpect(! compositionBundle.empty(), HttpStatus::BadRequest,
                      constraintError(KbvBundleConstraint::erp_angabeRechtsgrundlage));
            for (const auto& composition : compositionBundle)
            {
                const auto& ext = composition.getExtension<model::KBVEXFORLegalBasis>();
                ErpExpect(ext.has_value(), HttpStatus::BadRequest,
                          constraintError(KbvBundleConstraint::erp_angabeRechtsgrundlage));
            }
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFail(HttpStatus::BadRequest, constraintError(KbvBundleConstraint::erp_angabeRechtsgrundlage, ex.what()));
    }
}


void KbvBundleValidator_V1_0_2::ikKostentraegerBgUkPflicht(const model::KbvBundle& kbvBundle) const
{
    // IK-Kostentraeger-BG-UK-Pflicht
    // (entry.where(resource is Coverage).exists()
    // and (entry.where(resource is Coverage).resource.type.coding.code='BG'
    //      or entry.where(resource is Coverage).resource.type.coding.code='UK'))
    // implies  entry.select(resource as Coverage).payor.identifier
    //  .extension('https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK').exists()
    try
    {
        if (isKostentraeger(kbvBundle, {"BG", "UK"}))
        {
            const auto& coverageBundle = kbvBundle.getResourcesByType<model::KbvCoverage>();
            for (const auto& coverage : coverageBundle)
            {
                ErpExpect(coverage.hasPayorIdentifierExtension(
                              "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK"),
                          HttpStatus::BadRequest, constraintError(KbvBundleConstraint::IK_Kostentraeger_BG_UK_Pflicht));
            }
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFail(HttpStatus::BadRequest,
                constraintError(KbvBundleConstraint::IK_Kostentraeger_BG_UK_Pflicht, ex.what()));
    }
}


void KbvBundleValidator_V1_0_2::validateMedicationProfiles(const model::KbvBundle& kbvBundle,
                                                           const XmlValidator& xmlValidator,
                                                           const InCodeValidator& inCodeValidator) const
{
    // there are four different profiles for the Medication Resource, which cannot be distinguished in xsd.
    const auto& medications = kbvBundle.getResourcesByType<model::KbvMedicationGeneric>();
    const auto& version = model::ResourceVersion::deprecated<model::ResourceVersion::KbvItaErp>();
    for (const auto& medication : medications)
    {
        const auto profile = medication.getProfile();
        switch (profile)
        {
            case SchemaType::KBV_PR_ERP_Medication_Compounding:
                (void) model::KbvMedicationCompounding::fromXml(
                    medication.serializeToXmlString(), xmlValidator, inCodeValidator, profile,
                    {model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01}, false, version);
                break;
            case SchemaType::KBV_PR_ERP_Medication_FreeText:
                (void) model::KbvMedicationFreeText::fromXml(
                    medication.serializeToXmlString(), xmlValidator, inCodeValidator, profile,
                    {model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01}, false, version);
                break;
            case SchemaType::KBV_PR_ERP_Medication_Ingredient:
                (void) model::KbvMedicationIngredient::fromXml(
                    medication.serializeToXmlString(), xmlValidator, inCodeValidator, profile,
                    {model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01}, false, version);
                break;
            case SchemaType::KBV_PR_ERP_Medication_PZN:
                (void) model::KbvMedicationPzn::fromXml(
                    medication.serializeToXmlString(), xmlValidator, inCodeValidator, profile,
                    {model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01}, false, version);
                break;
            default:
                ErpFail(HttpStatus::BadRequest, "Unsupported medication profile");
                break;
        }
    }
}

template<typename TResource>
void KbvBundleValidator_V1_0_2::validateEntry(const model::KbvBundle& kbvBundle, const XmlValidator& xmlValidator,
                                              const InCodeValidator& inCodeValidator, SchemaType schemaType) const
{
    const auto& entryBundle = kbvBundle.getResourcesByType<TResource>();
    for (const auto& entry : entryBundle)
    {
        //
        inCodeValidator.validate(entry, schemaType, xmlValidator);
    }
}

bool KbvBundleValidator_V1_0_2::isKostentraeger(const model::KbvBundle& kbvBundle,
                                                std::set<std::string> kostentraeger) const
{
    const auto& coverageBundle = kbvBundle.getResourcesByType<model::KbvCoverage>();
    return std::any_of(coverageBundle.begin(), coverageBundle.end(), [&kostentraeger](const auto& coverage) {
        return KbvValidationUtils::isKostentraeger(coverage, kostentraeger);
    });
}


void KbvBundleValidator_V1_0_2::validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                          const InCodeValidator& inCodeValidator) const
{
    const auto& kbvBundle = dynamic_cast<const model::KbvBundle&>(resource);
    erp_compositionPflicht(kbvBundle);
    erp_angabePruefnummer(kbvBundle);
    erp_angabeZuzahlung(kbvBundle);
    erp_angabePLZ(kbvBundle);
    erp_angabeNrAusstellendePerson(kbvBundle);
    erp_angabeBestriebsstaettennr(kbvBundle);
    erp_angabeRechtsgrundlage(kbvBundle);
    // -erp-versionComposition is enforced by xsd
    ikKostentraegerBgUkPflicht(kbvBundle);

    validateEntries(kbvBundle, xmlValidator, inCodeValidator);
}

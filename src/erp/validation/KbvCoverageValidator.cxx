/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/validation/KbvCoverageValidator.hxx"
#include "erp/model/Extension.hxx"
#include "erp/model/KbvCoverage.hxx"
#include "erp/validation/KbvValidationUtils.hxx"

namespace
{
enum class KbvCoverageConstraint
{
    IK_zustaendige_Krankenkasse_eGK,
    Versichertenart_Pflicht,
    Besondere_Personengruppe_Pflicht,
    IK_Kostentraeger_BG_UK,
    DMP_Pflicht,
    KBVdate
};
std::string constraintMessage(KbvCoverageConstraint constraint)
{
    switch (constraint)
    {
        case KbvCoverageConstraint::IK_zustaendige_Krankenkasse_eGK:
            return "Das IK der zustaendigen Krankenkasse lt. eGK ist nicht vorhanden, sie ist aber bei den "
                   "Kostentraegern vom Typ 'GKV', 'BG', 'SKT', 'PKV' oder 'UK' ein Pflichtelement.";
        case KbvCoverageConstraint::Versichertenart_Pflicht:
            return "Die Versichertenart ist nicht vorhanden, sie ist aber bei den Kostentraegern vom Typ 'GKV', 'BG', "
                   "'SKT', 'PKV' oder 'UK' ein Pflichtelement.";
        case KbvCoverageConstraint::Besondere_Personengruppe_Pflicht:
            return "Die Besondere Personengruppe ist nicht vorhanden, sie ist aber bei den Kostentraegern vom Typ "
                   "'GKV', 'BG', 'SKT', 'PKV' oder 'UK' ein Pflichtelement.";
        case KbvCoverageConstraint::IK_Kostentraeger_BG_UK:
            return "Nur wenn der Kostentraeger vom Typ BG oder UK ist, dann kann das IK des Kostentraegers uebertragen "
                   "werden";
        case KbvCoverageConstraint::DMP_Pflicht:
            return "Das DMP-Kennzeichen ist nicht vorhanden, dieses ist aber bei den Kostentraegern vom Typ 'GKV', "
                   "'BG', 'SKT', 'PKV' oder 'UK' ein Pflichtelement.";
        case KbvCoverageConstraint::KBVdate:
            return "Beschränkung auf die Angaben JJJJ-MM-TT";
    }
    Fail("invalid KbvCoverageConstraint " + std::to_string(static_cast<std::uintmax_t>(constraint)));
}
std::string constraintError(KbvCoverageConstraint constraint,
                            const std::optional<std::string>& additional = std::nullopt)
{
    return KbvValidationUtils::constraintError(constraintMessage(constraint), additional);
}
}

void KbvCoverageValidator::validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                                    const InCodeValidator& inCodeValidator) const
{
    doValidate(dynamic_cast<const model::KbvCoverage&>(resource), xmlValidator, inCodeValidator);
}


void KbvCoverageValidator_V1_0_2::doValidate(const model::KbvCoverage& kbvCoverage, const XmlValidator&,
                                             const InCodeValidator&) const
{
    KbvValidationUtils::checkKbvExtensionValueCoding(
        "http://fhir.de/StructureDefinition/gkv/besondere-personengruppe", kbvCoverage, false,
        {"https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PERSONENGRUPPE"}, {"00", "04", "06", "07", "08", "09"});
    KbvValidationUtils::checkKbvExtensionValueCoding(
        "http://fhir.de/StructureDefinition/gkv/dmp-kennzeichen", kbvCoverage, false,
        {"https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"},
        {"00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11"});
    KbvValidationUtils::checkKbvExtensionValueCoding(
        "http://fhir.de/StructureDefinition/gkv/versichertenart", kbvCoverage, false,
        {"https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS"}, {"1", "3", "5"});
    KbvValidationUtils::checkKbvExtensionValueCoding("http://fhir.de/StructureDefinition/gkv/wop", kbvCoverage, false,
                                                     {"https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_ITA_WOP"},
                                                     {"00", "01", "02", "03", "17", "20", "38", "46", "47",
                                                      "48", "49", "50", "51", "52", "55", "60", "61", "62",
                                                      "71", "72", "73", "78", "83", "88", "93", "98"});

    const auto* payorIdentifier = kbvCoverage.payorIdentifier();
    if (payorIdentifier)
    {
        const auto& payorIdentifierResource = model::UnspecifiedResource::fromJson(*payorIdentifier);
        KbvValidationUtils::checkKbvExtensionValueIdentifier(
            "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK", payorIdentifierResource, false,
            "http://fhir.de/NamingSystem/arge-ik/iknr", 9);

        const auto& extension = KbvValidationUtils::checkGetExtension(
            "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK", payorIdentifierResource, false);
        const auto& valueIdentifierTypeSystem = extension->valueIdentifierTypeSystem();
        if (valueIdentifierTypeSystem)
        {
            ErpExpectWithDiagnostics(valueIdentifierTypeSystem == "http://terminology.hl7.org/CodeSystem/v2-0203",
                                     HttpStatus::BadRequest,
                                     "type.coding[0].system has wrong value in extension "
                                     "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK",
                                     std::string(*valueIdentifierTypeSystem));
        }
        const auto& valueIdentifierTypeCode = extension->valueIdentifierTypeCode();
        if (valueIdentifierTypeCode)
        {
            ErpExpectWithDiagnostics(valueIdentifierTypeCode == "XX", HttpStatus::BadRequest,
                                     "type.coding[0].code has wrong value in extension "
                                     "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK",
                                     std::string(*valueIdentifierTypeCode));
        }
        const auto& valueIdentifierUse = extension->valueIdentifierUse();
        if (valueIdentifierUse)
        {
            ErpExpectWithDiagnostics(
                valueIdentifierUse == "official", HttpStatus::BadRequest,
                "use has wrong value in extension https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK",
                std::string(*valueIdentifierUse));
        }
    }


    IK_zustaendige_Krankenkasse_eGK(kbvCoverage);
    Versichertenart_Pflicht(kbvCoverage);
    Besondere_Personengruppe_Pflicht(kbvCoverage);
    IK_Kostentraeger_BG_UK(kbvCoverage);
    DMP_Pflicht(kbvCoverage);
    KBVdate(kbvCoverage);
    ik_1(kbvCoverage);
}

void KbvCoverageValidator_V1_0_2::IK_zustaendige_Krankenkasse_eGK(const model::KbvCoverage& kbvCoverage) const
{
    // IK-zustaendige-Krankenkasse-eGK
    // (type.coding.code='GKV'
    //  or type.coding.code='BG'
    //  or type.coding.code='SKT'
    //  or type.coding.code='UK'
    //  or type.coding.code='PKV')
    // implies payor.identifier.exists()
    try
    {
        if (KbvValidationUtils::isKostentraeger(kbvCoverage, {{"GKV", "BG", "SKT", "UK", "PKV"}}))
        {
            const auto& payorIdentifierValue = kbvCoverage.payorIdentifierValue();
            ErpExpect(payorIdentifierValue, HttpStatus::BadRequest,
                      constraintError(KbvCoverageConstraint::IK_zustaendige_Krankenkasse_eGK));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest,
                               constraintError(KbvCoverageConstraint::IK_zustaendige_Krankenkasse_eGK), ex.what());
    }
}

void KbvCoverageValidator_V1_0_2::Versichertenart_Pflicht(const model::KbvCoverage& kbvCoverage) const
{
    // Versichertenart-Pflicht
    // (type.coding.code='GKV'
    //  or type.coding.code='BG'
    //  or type.coding.code='SKT'
    //  or type.coding.code='UK'
    //  or type.coding.code='PKV')
    // implies extension('http://fhir.de/StructureDefinition/gkv/versichertenart').exists()
    try
    {
        if (KbvValidationUtils::isKostentraeger(kbvCoverage, {{"GKV", "BG", "SKT", "UK", "PKV"}}))
        {
            const auto& versichertenartExtension =
                kbvCoverage.getExtension<model::Extension>("http://fhir.de/StructureDefinition/gkv/versichertenart");
            ErpExpect(versichertenartExtension, HttpStatus::BadRequest,
                      constraintError(KbvCoverageConstraint::Versichertenart_Pflicht));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, constraintError(KbvCoverageConstraint::Versichertenart_Pflicht),
                               ex.what());
    }
}

void KbvCoverageValidator_V1_0_2::Besondere_Personengruppe_Pflicht(const model::KbvCoverage& kbvCoverage) const
{
    // Besondere-Personengruppe-Pflicht
    // (type.coding.code='GKV'
    //  or type.coding.code='BG'
    //  or type.coding.code='SKT'
    //  or type.coding.code='UK'
    //  or type.coding.code='PKV')
    // implies extension('http://fhir.de/StructureDefinition/gkv/besondere-personengruppe').exists()
    try
    {
        if (KbvValidationUtils::isKostentraeger(kbvCoverage, {{"GKV", "BG", "SKT", "UK", "PKV"}}))
        {
            const auto& besonderePersonengruppeExtension = kbvCoverage.getExtension<model::Extension>(
                "http://fhir.de/StructureDefinition/gkv/besondere-personengruppe");
            ErpExpect(besonderePersonengruppeExtension, HttpStatus::BadRequest,
                      constraintError(KbvCoverageConstraint::Besondere_Personengruppe_Pflicht));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest,
                               constraintError(KbvCoverageConstraint::Besondere_Personengruppe_Pflicht), ex.what());
    }
}

void KbvCoverageValidator_V1_0_2::IK_Kostentraeger_BG_UK(const model::KbvCoverage& kbvCoverage) const
{
    // IK-Kostentraeger-BG-UK
    // (type.coding.code='GKV'
    //  or type.coding.code='SKT'
    //  or type.coding.code='SEL'
    //  or type.coding.code='PKV')
    // implies payor.identifier.extension('https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK').exists().not()
    try
    {
        if (KbvValidationUtils::isKostentraeger(kbvCoverage, {"GKV", "SKT", "SEL", "PKV"}))
        {
            const auto alternativeIkExtension = kbvCoverage.hasPayorIdentifierExtension(
                "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK");
            ErpExpect(! alternativeIkExtension, HttpStatus::BadRequest,
                      constraintError(KbvCoverageConstraint::IK_Kostentraeger_BG_UK));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, constraintError(KbvCoverageConstraint::IK_Kostentraeger_BG_UK),
                               ex.what());
    }
}

void KbvCoverageValidator_V1_0_2::DMP_Pflicht(const model::KbvCoverage& kbvCoverage) const
{
    // DMP_Pflicht
    // (type.coding.code='GKV'
    //  or type.coding.code='BG'
    //  or type.coding.code='SKT'
    //  or type.coding.code='UK'
    //  or type.coding.code='PKV')
    // implies extension('http://fhir.de/StructureDefinition/gkv/dmp-kennzeichen').exists()
    try
    {
        if (KbvValidationUtils::isKostentraeger(kbvCoverage, {{"GKV", "BG", "SKT", "UK", "PKV"}}))
        {
            const auto& dmpKennzeichenExtension =
                kbvCoverage.getExtension<model::Extension>("http://fhir.de/StructureDefinition/gkv/dmp-kennzeichen");
            ErpExpect(dmpKennzeichenExtension, HttpStatus::BadRequest,
                      constraintError(KbvCoverageConstraint::DMP_Pflicht));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, constraintError(KbvCoverageConstraint::DMP_Pflicht), ex.what());
    }
}

void KbvCoverageValidator_V1_0_2::KBVdate(const model::KbvCoverage& kbvCoverage) const
{
    // KBVdate
    // end.toString().length() = 10
    try
    {
        const auto& periodEnd = kbvCoverage.periodEnd();
        if (periodEnd)
        {
            ErpExpect(periodEnd->size() == 10, HttpStatus::BadRequest, constraintError(KbvCoverageConstraint::KBVdate));
        }
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, constraintError(KbvCoverageConstraint::KBVdate), ex.what());
    }
}

void KbvCoverageValidator_V1_0_2::ik_1(const model::KbvCoverage& kbvCoverage) const
{
    // ik-1
    // matches('[0-9]{8,9}')
    const auto& payorIdentifierValue = kbvCoverage.payorIdentifierValue();
    if (payorIdentifierValue)
    {
        KbvValidationUtils::checkIknr(*payorIdentifierValue);
    }
}

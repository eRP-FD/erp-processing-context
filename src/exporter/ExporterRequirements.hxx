/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EXPORTERREQUIREMENTS_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EXPORTERREQUIREMENTS_HXX

#include "shared/util/Requirement.hxx"

// clang-format off
[[maybe_unused]] static Requirement A_25940("E-Rezept-Fachdienst - ePA - Aktualisierung Cache Zuordnung KVNR zu ePA-Aktensystem");
[[maybe_unused]] static Requirement A_25937("E-Rezept-Fachdienst - ePA - Lokalisierung des ePA-Aktensysteme");
[[maybe_unused]] static Requirement A_25938("E-Rezept-Fachdienst - ePA - Aktualisierung Cache ePA-Aktensysteme");
[[maybe_unused]] static Requirement A_25951("E-Rezept-Fachdienst - ePA - Prüfung des Widerspruchs vor Übermittlung");
[[maybe_unused]] static Requirement A_25946("E-Rezept-Fachdienst - ePA - Mapping");
[[maybe_unused]] static Requirement A_25947("E-Rezept-Fachdienst - ePA - provide-dispensation-erp - Organisation-Ressource");
[[maybe_unused]] static Requirement A_25948("E-Rezept-Fachdienst - ePA - Mapping - Übernahme von Werten zwischen Profilen");
[[maybe_unused]] static Requirement A_25949("E-Rezept-Fachdienst - ePA - Mapping - Handhabung von Extensions");

// Rules from https://wiki.gematik.de/pages/viewpage.action?spaceKey=B714ERPFD&title=FHIR+Transformator+-+Mapping+der+Profile
[[maybe_unused]] static Requirement F_001("Semantik authoredOn Parameter in ePA Medication Service ANFERP-2605");
[[maybe_unused]] static Requirement F_002("Regel für Referenzen ANFERP-2604");
//TODO: [[maybe_unused]] static Requirement F_003("TODO - Regel für Resource Ids (neu generieren? aus quell profilen übernehmen?");
[[maybe_unused]] static Requirement F_004("KBV_PR_ERP_Prescription|1.1.0 in EPAMedicationRequest|1.1.0-rc1 → MedicationRequest.extension:Mehrfachverordnung ANFERP-2633");
[[maybe_unused]] static Requirement F_005("Mapping von Extensions und CodeSystems ANFERP-2637");
[[maybe_unused]] static Requirement F_006a("Defaultwerte bei Organization (provideDispensation) ANFERP-2604");
[[maybe_unused]] static Requirement F_006b("Mapping der Organization (providePrescrition) ANFERP-2604");
[[maybe_unused]] static Requirement F_007("Mapping Organization");
[[maybe_unused]] static Requirement F_008("KBV_PR_ERP_Medication_Compounding|1.1.0 und KBV_PR_ERP_Medication_Ingredient Mapping Medication.ingredient.strength");
[[maybe_unused]] static Requirement F_009("KBV_PR_ERP_Medication_Compounding|1.1.0 Mapping Medication.ingredient.strength ANFERP-2617");
[[maybe_unused]] static Requirement F_010("MedicationRequest.subject ANFERP-2638");
// TODO: [[maybe_unused]] static Requirement F_011("Practioner.name.text ANFERP-2639");
[[maybe_unused]] static Requirement F_012("NOT A REQUIREMENT? Unterstrich im key z.B “_system” im in dem Beispiel Medication-Medication-Without-Strength-Numerator.json ANFERP-2617");
[[maybe_unused]] static Requirement F_013("Telematik-ID im Practitioner");
[[maybe_unused]] static Requirement F_014("Mapping MedicationDispense.subject.identifier ANFERB-2779");
[[maybe_unused]] static Requirement F_015("Medication.code.coding");
[[maybe_unused]] static Requirement F_016("Zwei Practitioner im kbv_pr_erp_bundle ANFERB-2780");
[[maybe_unused]] static Requirement F_017("KBV_PR_ERP_Medication_Compounding  mapping von itemCodeableConcept.coding - PZN ANFERP-2911");
// clang-format on

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EXPORTERREQUIREMENTS_HXX

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EXPORTERREQUIREMENTS_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EXPORTERREQUIREMENTS_HXX

#include "shared/util/Requirement.hxx"

// clang-format off
static constexpr Requirement A_25940("E-Rezept-Fachdienst - ePA - Aktualisierung Cache Zuordnung KVNR zu ePA-Aktensystem");
static constexpr Requirement A_25941("E-Rezept-Fachdienst - ePA - Aktualisierung Cache Zuordnung KVNR zu ePA-Aktensystem - Statuscode 404");
static constexpr Requirement A_25942("E-Rezept-Fachdienst - ePA - Fehlerbehandlung - Information Service - Fehler im Information Service");
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
[[maybe_unused]] static Requirement F_010("MedicationRequest.subject ANFERP-2638");
// TODO: [[maybe_unused]] static Requirement F_011("Practioner.name.text ANFERP-2639");
[[maybe_unused]] static Requirement F_012("NOT A REQUIREMENT? Unterstrich im key z.B “_system” im in dem Beispiel Medication-Medication-Without-Strength-Numerator.json ANFERP-2617");
[[maybe_unused]] static Requirement F_013("Telematik-ID im Practitioner");
[[maybe_unused]] static Requirement F_014("Mapping MedicationDispense.subject.identifier ANFERB-2779");
[[maybe_unused]] static Requirement F_015("Medication.code.coding");
[[maybe_unused]] static Requirement F_016("Zwei Practitioner im kbv_pr_erp_bundle ANFERB-2780");
[[maybe_unused]] static Requirement F_017("KBV_PR_ERP_Medication_Compounding  mapping von itemCodeableConcept.coding - PZN ANFERP-2911");
[[maybe_unused]] static const Requirement F_019("no mapping of Practitioner.qualification");
[[maybe_unused]] static const Requirement F_020("Medication-Type-Extension in EPAMedications");
[[maybe_unused]] static const Requirement F_021("Add code and system defaults for kbv bundles");

static constexpr Requirement A_27882("E-Rezept-Fachdienst - BfArM - Lokalisierung Konfigurationsparameter BfArM_Domain");
static constexpr Requirement A_27817("E-Rezept-Fachdienst - BfArM - Lokalisierung des BfArM Webdienstes");
// static constexpr Requirement A_27819("Anbieter E-Rezept Fachdienst - BfArM - Registrierung für Client Credentials am BfArM Webdienst");
static constexpr Requirement A_27820("E-Rezept-Fachdienst - BfArM - Prüfung Gültigkeit AccessToken");
static constexpr Requirement A_27821("E-Rezept-Fachdienst - BfArM - Beziehen des AccessTokens");
static constexpr Requirement A_27822("E-Rezept-Fachdienst - BfArM - AccessToken für Zugriff auf den BfArM Webdienst");
static constexpr Requirement A_27823("E-Rezept-Fachdienst - BfArM - Flowtype 166");
static constexpr Requirement A_27824("E-Rezept-Fachdienst - BfArM - asynchrone Bereitstellung und Übermittlung");
static constexpr Requirement A_27825("E-Rezept-Fachdienst - BfArM - Suche nach Apothekendaten");
static constexpr Requirement A_27826("E-Rezept-Fachdienst - BfArM - Erzeugen digitaler Durchschlag E-T-Rezept");
static constexpr Requirement A_27827("E-Rezept-Fachdienst - BfArM - Anwendungsfall Übertragen des digitalen Durchschlags");
static constexpr Requirement A_27828("E-Rezept-Fachdienst - BfArM - Übertragen des digitalen Durchschlags");
static constexpr Requirement A_27830("E-Rezept-Fachdienst - BfArM - Fehlerbehandlung - Reaktion auf Scheitern des Operationaufrufs");
static constexpr Requirement A_27831("E-Rezept-Fachdienst - BfArM - Fehlerbehandlung - Protokollierung struktureller Fehler");

static constexpr Requirement A_27855("E-Rezept: Zugriff auf Webdienste - Webzertifikat aus Internet PKI");
static constexpr Requirement A_27856("E-Rezept: Zugriff auf Webdienste - Hostname-Prüfung für TLS-Server-Zertifikat durchführen");
static constexpr Requirement A_27857("E-Rezept: Zugriff auf Webdienste - Sperrprüfung für TLS-Server-Zertifikat durchführen");
static constexpr Requirement A_27858("E-Rezept: Zugriff auf Webdienste - TLS-Protokollversion und Cipher Suites");

static constexpr Requirement A_27859("E-Rezept-Fachdienst - Zugriff auf Webdienste - Deaktivieren von Übertragungen");
static constexpr Requirement A_27860("Anbieter E-Rezept-Fachdienst - Zugriff auf Webdienste - BetrieblicherProzess Deaktivieren von Übertragungen");

// clang-format on

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_EXPORTERREQUIREMENTS_HXX

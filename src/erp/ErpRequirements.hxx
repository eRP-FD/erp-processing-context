/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_ERPREQUIREMENTS_HXX
#define ERP_PROCESSING_CONTEXT_ERPREQUIREMENTS_HXX

#include "erp/util/Requirement.hxx"

static Requirement A_20171    ("E-Rezept-Fachdienst - RESTful API CapabilityStatement");
static Requirement A_20744    ("E-Rezept-Fachdienst - Selbstauskunft Device-Informationen");
static Requirement A_19514    ("E-Rezept-Fachdienst - Http-Status-Codes");
static Requirement A_19537    ("E-Rezept-Fachdienst - RESTful API MimeType fhir+xml");
static Requirement A_19538    ("E-Rezept-Fachdienst - RESTful API MimeType fhir+json");
static Requirement A_19539    ("E-Rezept-Fachdienst - RESTful API MimeType Aufrufparameter");

// Requirements Allgemeine Sicherheitsanforderungen
static Requirement A_20703    ("E-Rezept-Fachdienst - Drosselung Brute-Force-Anfragen");
static Requirement A_20704    ("E-Rezept-Fachdienst - Drosselung Rezeptfälschungen");
static Requirement A_19700    ("E-Rezept-Fachdienst - Ableitung der Persistenzschluessel aus Merkmal der E-Rezepte");
static Requirement A_19688    ("E-Rezept-Fachdienst - Verschluesselung von ausserhalb des Verarbeitungskontextes der VAU gespeicherten Daten");
static Requirement A_20751    ("E-Rezept-Fachdienst - Erkennen der Prüfidentität");
static Requirement A_20752    ("E-Rezept-Fachdienst - Ausschluss Vertreterkommunikation von bzw. an Prüf-Identität");
static Requirement A_20753    ("E-Rezept-Fachdienst - Ausschluss Vertreterzugriff auf bzw. mittels Prüf-Identität");

// Requirements for the professionOID Rollenprüfung
static Requirement A_19018    ("E-Rezept-Fachdienst - Rollenpruefung Verordnender stellt Rezept ein");
static Requirement A_19022    ("E-Rezept-Fachdienst - Rollenpruefung Verordnender aktiviert Rezept");
static Requirement A_19026    ("E-Rezept-Fachdienst - Rollenpruefung Nutzer loescht Rezept");
static Requirement A_19113_01 ("E-Rezept-Fachdienst - Rollenpruefung Versicherter oder Apotheker liest Rezept");
static Requirement A_21558    ("E-Rezept-Fachdienst - Rollenprüfung Versicherter liest Rezepte");
static Requirement A_19166    ("E-Rezept-Fachdienst - Rollenpruefung Abgebender ruft Rezept ab");
static Requirement A_19170_01 ("E-Rezept-Fachdienst - Rollenprüfung Abgebender weist zurück");
static Requirement A_19230    ("E-Rezept-Fachdienst - Rollenpruefung Abgebender vollzieht Abgabe des Rezepts");
static Requirement A_19389    ("E-Rezept-Fachdienst - Authentifizierung erforderlich Vers-Endpunkt");
static Requirement A_19390    ("E-Rezept-Fachdienst - Authentifizierung Nutzerrolle");
static Requirement A_19391    ("E-Rezept-Fachdienst - Authentifizierung Nutzername");
static Requirement A_19392    ("E-Rezept-Fachdienst - Authentifizierung Nutzerkennung");
static Requirement A_19439    ("E-Rezept-Fachdienst - Authentifizierung Authentifizierungsstärke");
static Requirement A_19395    ("E-Rezept-Fachdienst - Rollenpruefung Versicherter liest AuditEvent");
static Requirement A_19405    ("E-Rezept-Fachdienst - Rollenpruefung Versicherter liest MedicationDispense");
static Requirement A_19446_01 ("E-Rezept-Fachdienst - Rollenpruefung Versicherter oder Apotheker liest Rezept");
// PKV related:
static Requirement A_22113    ("E-Rezept-Fachdienst - Abrechnungsinformation löschen - Rollenprüfung");
static Requirement A_22118    ("E-Rezept-Fachdienst - Abrechnungsinformationen lesen - Rollenprüfung Versicherter oder Apotheker");
static Requirement A_22124    ("E-Rezept-Fachdienst - Abrechnungsinformationen lesen - Rollenprüfung Versicherter oder Apotheker");
static Requirement A_22129    ("E-Rezept-Fachdienst - Abrechnungsinformation bereitstellen - Rollenprüfung");
static Requirement A_22144    ("E-Rezept-Fachdienst - Abrechnungsinformation ändern – Rollenprüfung");
static Requirement A_22155    ("E-Rezept-Fachdienst - Consent löschen - Rollenprüfung Versicherter");
static Requirement A_22159    ("E-Rezept-Fachdienst - Consent lesen - Rollenprüfung Versicherter");
static Requirement A_22161    ("E-Rezept-Fachdienst - Consent schreiben - Rollenprüfung Versicherter");

// Requirements for Communications
static Requirement A_19401    ("E-Rezept-Fachdienst - unzulässige Operationen Communication");
static Requirement A_19447    ("E-Rezept-Fachdienst - Nachricht einstellen Schemaprüfung");
static Requirement A_19448    ("E-Rezept-Fachdienst - Nachricht einstellen Absender und Sendedatum");
static Requirement A_19450    ("E-Rezept-Fachdienst - Nachricht einstellen Schadcodeprüfung");
static Requirement A_19520    ("E-Rezept-Fachdienst - Nachrichten für Empfänger filtern");
static Requirement A_19521    ("E-Rezept-Fachdienst - Nachrichten als abgerufen markieren");
static Requirement A_19522    ("E-Rezept-Fachdienst - Nachrichtenabruf Suchparameter");
static Requirement A_19534    ("E-Rezept-Fachdienst - Rückgabe Communication im Bundle Paging");
static Requirement A_20229    ("E-Rezept-Fachdienst - Nachrichtenzähler bei Versicherter-zu-Versichertem-Kommunikation");
static Requirement A_20230    ("E-Rezept-Fachdienst - Einlösbare E-Rezepte für Versicherter-zu-Versichertem-Kommunikation");
static Requirement A_20231    ("E-Rezept-Fachdienst - Ausschluss Nachrichten an Empfänger gleich Absender");
static Requirement A_20258    ("E-Rezept-Fachdienst - Communication löschen auf Basis Absender-ID");
static Requirement A_20259    ("E-Rezept-Fachdienst - Communication löschen mit Warnung wenn vom Empfänger bereits abgerufen");
static Requirement A_20260    (R"(Anwendungsfall "Nachricht durch Versicherten löschen")");
static Requirement A_20776    (R"(Anwendungsfall "Nachricht durch Abgebenden löschen")");
static Requirement A_20885_01 ("E-Rezept-Fachdienst - Nachricht einstellen Prüfung Versichertenbezug und Berechtigung");

// Requirements for "Claims der Identitätsbestätigung"
static Requirement A_19130    ("E-Rezept-Fachdienst - Authentifizierung erforderlich LEI-Endpunkt");
static Requirement A_19131    ("E-Rezept-Fachdienst - Authentifizierung ungültig");
static Requirement A_19132    ("E-Rezept-Fachdienst - Authentifizierung Signaturprüfung");
static Requirement A_19189    ("E-Rezept-Fachdienst - Authentifizierung erforderlich Vers-Endpunkt");
static Requirement A_19902    ("E-Rezept-Fachdienst - Authentifizierung abgelaufen");
static Requirement A_19992    ("E-Rezept-Fachdienst - Blacklisting zu haeufig verwendeter ACCESS_TOKEN");
static Requirement A_19993    ("E-Rezept-Fachdienst - Prüfung eingehender ACCESS_TOKEN");

// Requirements for TEE (VAU) protocol.
static Requirement A_20160    ("E-Rezept-VAU, Schlüsselpaar und Zertifikat");
static Requirement A_20161_01 ("E-Rezept-Client, Request-Erstellung");
static Requirement A_20162    ("E-Rezept-FD, Webschnittstellen, VAU-Requests");
static Requirement A_20163    ("E-Rezept-VAU, Nutzeranfrage, Ent-und Verschlüsselung");
static Requirement A_20174    ("E-Rezept-Client, Response-Auswertung");
static Requirement A_20175    ("E-Rezept-Client, Speicherung  Nutzerpseudonym");

// Requirements for Prescription ID (Rezept-ID)
static Requirement A_19217    ("Aufbau E-Rezept-ID");
static Requirement A_19218    ("Prüfung E-Rezept-ID");
static Requirement A_19019    ("E-Rezept-Fachdienst - Generierung Rezept-ID");

// Requirements for endpoints GET /Task and GET /Task/{id}
static Requirement A_19115    ("E-Rezept-Fachdienst - Filter Tasks auf KVNR des Versicherten");
static Requirement A_19116    ("E-Rezept-Fachdienst - Prüfung AccessCode bei KVNR-Missmatch");
static Requirement A_19129_01 ("E-Rezept-Fachdienst - Rückgabe Tasks im Bundle Versicherter");
static Requirement A_21375    ("E-Rezept-Fachdienst - Rückgabe Task inkl. Bundles Versicherter");
static Requirement A_20702_01("E-Rezept-Fachdienst - Keine Einlöseinformationen in unbekannten Clients");
static Requirement A_19226    ("E-Rezept-Fachdienst - Rückgabe Task inkl. Bundle im Bundle Apotheker");
static Requirement A_19569_02 ("E-Rezept-Fachdienst - Suchparameter Task");
static Requirement A_18952    ( R"(E-Rezept-Fachdienst – Abfrage E-Rezept mit Status "gelöscht")");
static Requirement A_19030    ("E-Rezept-Fachdienst - unzulaessige Operationen Task");

// Requirements for endpoint POST /Task/$create
static Requirement A_19021    ("E-Rezept-Fachdienst - Generierung AccessCode");
static Requirement A_19112    ("E-Rezept-Fachdienst - Parametrierung Task fuer Workflow-Typ");
static Requirement A_19214    ("E-Rezept-Fachdienst - Ergaenzung Performer-Typ fuer Einloeseinstitutstyp");
static Requirement A_21265    ("Prozessparameter - Flowtype"); ///< supersedes A_19445
static Requirement A_19114    ("E-Rezept-Fachdienst - Status draft");
static Requirement A_19257    ("E-Rezept-Fachdienst - Schemavalidierung Rezept anlegen");

// Requirements for endpoint POST /Task/{id}/$activate
static Requirement A_19024_01 ("E-Rezept-Fachdienst - Pruefung AccessCode Rezept aktualisieren");
static Requirement A_19128    ("E-Rezept-Fachdienst - Status aktivieren");
static Requirement A_19020    ("E-Rezept-Fachdienst - Schemavalidierung Rezept aktivieren");
static Requirement A_19025    ("E-Rezept-Fachdienst - QES pruefen Rezept aktualisieren");
static Requirement A_19029_03 ("E-Rezept-Fachdienst - Serversignatur Rezept aktivieren");
static Requirement A_19127    ("E-Rezept-Fachdienst - Uebernahme der KVNR des Patienten");
static Requirement A_19225    ("E-Rezept-Fachdienst - QES durch berechtigte Berufsgruppe");
static Requirement A_19999    ("E-Rezept-Fachdienst - Ergaenzung PerformerTyp fuer Einloeseinstitutstyp");
static Requirement A_20159    ("E-Rezept-Fachdienst - QES Pruefung Signaturzertifikat des HBA");
static Requirement A_19517_02 ("FHIR FLOWTYPE für Prozessparameter - Abweichende Festlegung für Entlassrezept");
static Requirement A_22222    ("E-Rezept-Fachdienst - Ausschluss weitere Kostenträger");

// Requirements for endpoint POST /Task/$accept
static Requirement A_19149    ("E-Rezept-Fachdienst - Prüfung Datensatz zwischenzeitlich gelöscht");
static Requirement A_19167_01 ("E-Rezept-Fachdienst - Prüfung AccessCode Rezept abrufen");
static Requirement A_19168    ("E-Rezept-Fachdienst - Rezept bereits in Abgabe oder Bearbeitung");
static Requirement A_19169    ("E-Rezept-Fachdienst - Generierung Secret, Statuswechsel in Abgabe und Rückgabewert");

// Requirements for endpoint POST /Task/$close
static Requirement A_19231    ("E-Rezept-Fachdienst - Prüfung Secret Rezept beenden");
static Requirement A_19248_01 ("E-Rezept-Fachdienst - Schemaprüfung und Speicherung MedicationDispense");
static Requirement A_19232    ("E-Rezept-Fachdienst - Status beenden");
static Requirement A_20513    ("E-Rezept-Fachdienst - nicht mehr benötigte Einlösekommunikation");
static Requirement A_19233    ("E-Rezept-Fachdienst - Serversignatur Rezept beenden (Quittung erstellen)");
static Requirement A_19233_03 ("E-Rezept-Fachdienst - Hashwert der Verordnung in Quittung hinzufügen");

// Requirements for endpoint POST /Task/$abort
static Requirement A_19145    ("E-Rezept-Fachdienst - Statusprüfung Apotheker löscht Rezept");
static Requirement A_19146    ("E-Rezept-Fachdienst - Statusprüfung Nutzer (außerhalb Apotheke) löscht Rezept");
static Requirement A_20546_01 ("E-Rezept-Fachdienst - Prüfung KVNR, Versicherter löscht RezeptE-Rezept-Fachdienst - Prüfung KVNR, Versicherter löscht Rezept");
static Requirement A_20547    ("E-Rezept-Fachdienst - Prüfung KVNR, Vertreter löscht Rezept");
static Requirement A_19120    ("E-Rezept-Fachdienst - Prüfung AccessCode, Verordnender löscht Rezept");
static Requirement A_19224    ("E-Rezept-Fachdienst - Prüfung Secret, Apotheker löscht Rezept");
static Requirement A_19027    ("E-Rezept-Fachdienst - Rezept löschen");
static Requirement A_19121    ("E-Rezept-Fachdienst - Finaler Status cancelled");

// Requirements for endpoint POST /Task/$reject
static Requirement A_19171    ("A_19171 - E-Rezept-Fachdienst - Prüfung Secret Rezept zurückweisen");
static Requirement A_19172    ("A_19172 - E-Rezept-Fachdienst - Löschung Secret und Status");

// Requirements for endpoint /AuditEvent and /AuditEvent/<id>
static Requirement A_19402   ("E-Rezept-Fachdienst - unzulässige Operationen AuditEvent");
static Requirement A_19396   ("E-Rezept-Fachdienst - Filter AuditEvent auf KVNR des Versicherten");
static Requirement A_19399   ("E-Rezept-Fachdienst - Suchparameter AuditEvent");
static Requirement A_19397   ("E-Rezept-Fachdienst - Rückgabe AuditEvents im Bundle");
static Requirement A_19398   ("E-Rezept-Fachdienst - Rückgabe AuditEvents im Bundle Paging");

// Requirements for endpoint /MedicationDispense
static Requirement A_19400("E-Rezept-Fachdienst - unzulaessige Operationen MedicationDispense");
static Requirement A_19406("E-Rezept-Fachdienst - Filter MedicationDispense auf KVNR des Versicherten");
static Requirement A_19518("E-Rezept-Fachdienst - Suchparameter für MedicationDispense");
static Requirement A_19140("Logische Operation - Dispensierinformationen durch Versicherten abrufen");
static Requirement A_19141("Logische Operation - Dispensierinformation für ein einzelnes E-Rezept abrufen");

// Requirements for endpoint /ChargeItem and /ChargeItem/<id>
static Requirement A_22111   ("E-Rezept-Fachdienst - ChargeItem - unzulässige Operationen");

// Requirements for endpoint /Consent and /Consent/<id>
static Requirement A_22153   ("E-Rezept-Fachdienst - unzulässige Operationen Consent");

// Requirements for ACCESS_TOKEN checks
static Requirement A_20362    ("E-Rezept-Fachdienst - 'ACCESS_TOKEN' generelle Struktur");
static Requirement A_20365    ("E-Rezept-Fachdienst - Die Signatur des ACCESS_TOKEN ist zu prüfen");
static Requirement A_20368    ("E-Rezept-Fachdienst - Auswertung der Claims");
static Requirement A_20370    ("E-Rezept-Fachdienst - Abbruch bei falschen Datentypen der Attribute");
static Requirement A_20372    ("E-Rezept-Fachdienst - Prüfung der zeitlichen Gültigkeit des ACCESS_TOKEN");
static Requirement A_20373    ("E-Rezept-Fachdienst - Prüfung der Gültigkeit des ACCESS_TOKEN für den Zugriff auf Fachdienste ohne nbf");
static Requirement A_20374    ("E-Rezept-Fachdienst - Prüfung der Gültigkeit des ACCESS_TOKEN für den Zugriff auf Fachdienste mit nbf");
static Requirement A_20504    ("E-Rezept-Fachdienst - Reaktion bei ungültiger oder fehlender Signatur des ACCESS_TOKEN");
static Requirement A_20974    ("E-Rezept-Fachdienst - Prüfungsintervall Signaturzertifikat E-Rezept-Fachdienst");
static Requirement A_20158_01 ("E-Rezept-Fachdienst - Pruefung Signaturzertifikat IdP");
static Requirement A_21520    ("E-Rezept-Fachdienst - Prüfung des aud Claim des ACCESS_TOKEN mit der vom Fachdienst registrierten URI");

// Transactions
static Requirement A_18936 ("E-Rezept-Fachdienst - Transaktionssicherheit");

// TSL/QES related requirements
static Requirement GS_A_4898     ("PKI - TSL-Grace-Period einer TSL");
static Requirement GS_A_4899     ("PKI - TSL Update-Prüfintervall");
static Requirement GS_A_5336     ("PKI - Zertifikatspruefung nach Ablauf TSL-Graceperiod");
static Requirement GS_A_4661_01  ("PKI - kritische Erweiterungen in Zertifikaten");

// C_10854: E-Rezept: Versionierung FHIR-Ressourcen
static Requirement A_22216 ("FHIR-Ressourcen Versionsangabe");

// Workflow 169
static Requirement A_21360 ("E-Rezept-Fachdienst - Keine Einlöseinformationen für Flowtype 169");
static Requirement A_21370 ("E-Rezept-Fachdienst - Prüfung Rezept-ID und Präfix gegen Flowtype");
static Requirement A_22068 ("E-Rezept-Fachdienst - Ausschluss Mehrfachverordnung");
static Requirement A_22102 ("E-Rezept-Fachdienst - E-Rezept löschen - Flowtype 169 - Versicherter - Statusprüfung");

// Requirements for endpoint /Subscription
static Requirement A_22362    ("E-Rezept-Fachdienst - Rollenpruefung Apotheke 'Create Subscription'");
static Requirement A_22363    ("E-Rezept-Fachdienst - Create Subscription 'validate telematic id'");
static Requirement A_22364    ("E-Rezept-Fachdienst - Create Subscription 'POST /Subscription response'");
static Requirement A_22365    ("E-Rezept-Fachdienst - Register Subscription 'pseudonym for telematic id'");
static Requirement A_22366    ("E-Rezept-Fachdienst - Create Subscription 'Bearer Token'");
static Requirement A_22367    ("E-Rezept-Fachdienst - Nachricht einstellen 'Notification Apotheke'");
static Requirement A_22383    ("E-Rezept-Fachdienst – Generierungsschlüssel – Pseudonym der Telematik-ID");

#endif

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ERPREQUIREMENTS_HXX
#define ERP_PROCESSING_CONTEXT_ERPREQUIREMENTS_HXX

#include "shared/util/Requirement.hxx"

static constexpr Requirement A_20171    ("E-Rezept-Fachdienst - RESTful API CapabilityStatement");
static constexpr Requirement A_20744    ("E-Rezept-Fachdienst - Selbstauskunft Device-Informationen");
static constexpr Requirement A_19514    ("E-Rezept-Fachdienst - Http-Status-Codes");
static constexpr Requirement A_19537    ("E-Rezept-Fachdienst - RESTful API MimeType fhir+xml");
static constexpr Requirement A_19538    ("E-Rezept-Fachdienst - RESTful API MimeType fhir+json");
static constexpr Requirement A_19539    ("E-Rezept-Fachdienst - RESTful API MimeType Aufrufparameter");

// Requirements Allgemeine Sicherheitsanforderungen
static constexpr Requirement A_20703    ("E-Rezept-Fachdienst - Drosselung Brute-Force-Anfragen");
static constexpr Requirement A_20704    ("E-Rezept-Fachdienst - Drosselung Rezeptfälschungen");
static constexpr Requirement A_19700    ("E-Rezept-Fachdienst - Ableitung der Persistenzschluessel aus Merkmal der E-Rezepte");
static constexpr Requirement A_19688    ("E-Rezept-Fachdienst - Verschluesselung von ausserhalb des Verarbeitungskontextes der VAU gespeicherten Daten");
static constexpr Requirement A_20751    ("E-Rezept-Fachdienst - Erkennen der Prüfidentität");
static constexpr Requirement A_20752    ("E-Rezept-Fachdienst - Ausschluss Vertreterkommunikation von bzw. an Prüf-Identität");
static constexpr Requirement A_20753    ("E-Rezept-Fachdienst - Ausschluss Vertreterzugriff auf bzw. mittels Prüf-Identität");

// Requirements for the professionOID Rollenprüfung
static constexpr Requirement A_19018    ("E-Rezept-Fachdienst - Rollenpruefung Verordnender stellt Rezept ein");
static constexpr Requirement A_19022    ("E-Rezept-Fachdienst - Rollenpruefung Verordnender aktiviert Rezept");
static constexpr Requirement A_19026    ("E-Rezept-Fachdienst - Rollenpruefung Nutzer loescht Rezept");
static constexpr Requirement A_19113_01 ("E-Rezept-Fachdienst - Rollenpruefung Versicherter oder Apotheker liest Rezept");
static constexpr Requirement A_21558_01 ("E-Rezept-Fachdienst - Rollenprüfung Versicherter liest Rezepte");
static constexpr Requirement A_19166_01 ("E-Rezept-Fachdienst - Task akzeptieren - Flowtype 160,169,200,209 - Rollenprüfung");
static constexpr Requirement A_25993    ("E-Rezept-Fachdienst - Task akzeptieren - Flowtype 162 - Rollenprüfung");
static constexpr Requirement A_19170_02 ("E-Rezept-Fachdienst - Rollenprüfung Abgebender weist zurück");
static constexpr Requirement A_19230_01 ("E-Rezept-Fachdienst - Rollenpruefung Abgebender vollzieht Abgabe des Rezepts");
static constexpr Requirement A_19389    ("E-Rezept-Fachdienst - Authentifizierung erforderlich Vers-Endpunkt");
static constexpr Requirement A_19390    ("E-Rezept-Fachdienst - Authentifizierung Nutzerrolle");
static constexpr Requirement A_19391_01 ("E-Rezept-Fachdienst - Authentifizierung Nutzername");
static constexpr Requirement A_19392    ("E-Rezept-Fachdienst - Authentifizierung Nutzerkennung");
static constexpr Requirement A_19439_02 ("E-Rezept-Fachdienst - Authentifizierung Authentifizierungsstärke");
static constexpr Requirement A_19395    ("E-Rezept-Fachdienst - Rollenpruefung Versicherter liest AuditEvent");
static constexpr Requirement A_19405    ("E-Rezept-Fachdienst - Rollenpruefung Versicherter liest MedicationDispense");
static constexpr Requirement A_19446_02 ("E-Rezept-Fachdienst - Rollenpruefung Versicherter oder Apotheker liest Rezept");
static constexpr Requirement A_24279    ("E-Rezept-Fachdienst - Rollenpruefung Abgebender stellt Dispensierinformationen bereit");
// PKV related Rollenprüfung:
static constexpr Requirement A_22113    ("E-Rezept-Fachdienst - Abrechnungsinformation löschen - Rollenprüfung");
static constexpr Requirement A_22118    ("E-Rezept-Fachdienst - Abrechnungsinformationen lesen - Rollenprüfung Versicherter oder Apotheker");
static constexpr Requirement A_22124    ("E-Rezept-Fachdienst - Abrechnungsinformationen lesen - Rollenprüfung Versicherter oder Apotheker");
static constexpr Requirement A_22129    ("E-Rezept-Fachdienst - Abrechnungsinformation bereitstellen - Rollenprüfung");
static constexpr Requirement A_22144    ("E-Rezept-Fachdienst - Abrechnungsinformation ändern – Rollenprüfung");
static constexpr Requirement A_22155    ("E-Rezept-Fachdienst - Consent loeschen - Rollenprüfung Versicherter");
static constexpr Requirement A_22159    ("E-Rezept-Fachdienst - Consent lesen - Rollenprüfung Versicherter");
static constexpr Requirement A_22161    ("E-Rezept-Fachdienst - Consent schreiben - Rollenprüfung Versicherter");

// Requirements for Communications
static constexpr Requirement A_19401    ("E-Rezept-Fachdienst - unzulässige Operationen Communication");
static constexpr Requirement A_19447_04 ("E-Rezept-Fachdienst - Nachricht einstellen - Schemaprüfung");
static constexpr Requirement A_19448_01 ("E-Rezept-Fachdienst - Nachricht einstellen - Absender und Sendedatum");
static constexpr Requirement A_19450_01 ("E-Rezept-Fachdienst - Nachricht einstellen Schadcodeprüfung");
static constexpr Requirement A_19520_01 ("E-Rezept-Fachdienst - Nachrichten abrufen - für Empfänger filtern");
static constexpr Requirement A_19521    ("E-Rezept-Fachdienst - Nachrichten als abgerufen markieren");
static constexpr Requirement A_19522    ("E-Rezept-Fachdienst - Nachrichtenabruf Suchparameter");
static constexpr Requirement A_19534    ("E-Rezept-Fachdienst - Rückgabe Communication im Bundle Paging");
static constexpr Requirement A_20229_01 ("E-Rezept-Fachdienst - Nachricht einstellen - Nachrichtenzähler bei Versicherter-zu-Versichertem-Kommunikation");
static constexpr Requirement A_20230_01 ("E-Rezept-Fachdienst - Nachricht einstellen - Einlösbare E-Rezepte für Versicherter-zu-Versichertem-Kommunikation");
static constexpr Requirement A_20231    ("E-Rezept-Fachdienst - Ausschluss Nachrichten an Empfänger gleich Absender");
static constexpr Requirement A_20258    ("E-Rezept-Fachdienst - Communication löschen auf Basis Absender-ID");
static constexpr Requirement A_20259    ("E-Rezept-Fachdienst - Communication löschen mit Warnung wenn vom Empfänger bereits abgerufen");
static constexpr Requirement A_20260    (R"(Anwendungsfall "Nachricht durch Versicherten löschen")");
static constexpr Requirement A_20776    (R"(Anwendungsfall "Nachricht durch Abgebenden löschen")");
static constexpr Requirement A_20885_03 ("E-Rezept-Fachdienst - Nachricht einstellen - Versicherte - Prüfung Versichertenbezug und Berechtigung");
static constexpr Requirement A_22731    ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Prüfung Flowtype Task");
static constexpr Requirement A_22734    ("E-Rezept-Fachdienst - Nachricht einstellen - Prüfung Existenz ChargeItem");
static constexpr Requirement A_23878    ("E-Rezept-Fachdienst - Nachricht einstellen - Validierung des Payload-Inhalt von GEM_ERP_PR_Communication_DispReq");
static constexpr Requirement A_23879    ("E-Rezept-Fachdienst - Nachricht einstellen - Validierung des Payload-Inhalt von GEM_ERP_PR_Communication_Reply");
static constexpr Requirement A_26320    ("E-Rezept-Fachdienst - Nachricht einstellen - Dispense Request - Prüfung Status Task");
static constexpr Requirement A_26321    ("E-Rezept-Fachdienst - Nachricht einstellen - Dispense Request - Prüfung Ende Gültigkeit Task");
static constexpr Requirement A_26327    ("E-Rezept-Fachdienst - Nachricht einstellen - Dispense Request - Prüfung Beginn Gültigkeit Task");

// Requirements for "Claims der Identitätsbestätigung"
static constexpr Requirement A_19130    ("E-Rezept-Fachdienst - Authentifizierung erforderlich LEI-Endpunkt");
static constexpr Requirement A_19131    ("E-Rezept-Fachdienst - Authentifizierung ungültig");
static constexpr Requirement A_19132    ("E-Rezept-Fachdienst - Authentifizierung Signaturprüfung");
static constexpr Requirement A_19189    ("E-Rezept-Fachdienst - Authentifizierung erforderlich Vers-Endpunkt");
static constexpr Requirement A_19992    ("E-Rezept-Fachdienst - Blocklisting zu häufig verwendeter ACCESS_TOKEN");
static constexpr Requirement A_19993_01 ("E-Rezept-Fachdienst - Prüfung eingehender ACCESS_TOKEN");

// Requirements for TEE (VAU) protocol.
static constexpr Requirement A_20160    ("E-Rezept-VAU, Schlüsselpaar und Zertifikat");
static constexpr Requirement A_20161_01 ("E-Rezept-Client, Request-Erstellung");
static constexpr Requirement A_20162    ("E-Rezept-FD, Webschnittstellen, VAU-Requests");
static constexpr Requirement A_20163    ("E-Rezept-VAU, Nutzeranfrage, Ent-und Verschlüsselung");
static constexpr Requirement A_20174    ("E-Rezept-Client, Response-Auswertung");
static constexpr Requirement A_20175    ("E-Rezept-Client, Speicherung  Nutzerpseudonym");

// Requirements for Prescription ID (Rezept-ID)
static constexpr Requirement A_19217_01 ("Aufbau E-Rezept-ID");
static constexpr Requirement A_19218    ("Prüfung E-Rezept-ID");
static constexpr Requirement A_19019_01 ("E-Rezept-Fachdienst - Task erzeugen - Generierung Rezept-ID");

// Requirements for endpoints GET /Task and GET /Task/{id}
static constexpr Requirement A_19115_01 ("E-Rezept-Fachdienst - Task abrufen - Filter Tasks auf KVNR des Versicherten");
static constexpr Requirement A_19116_01 ("E-Rezept-Fachdienst - Task abrufen - Prüfung AccessCode bei KVNR-Missmatch");
static constexpr Requirement A_19129_01 ("E-Rezept-Fachdienst - Rückgabe Tasks im Bundle Versicherter");
static constexpr Requirement A_21375_02 ("E-Rezept-Fachdienst - Task abrufen - Rückgabe Task inkl. Bundles Versicherter");
static constexpr Requirement A_21532_02 ("E-Rezept-Fachdienst - Task abrufen - Kein Secret für Versicherte");
static constexpr Requirement A_21371_02 ("E-Rezept-Fachdienst - Nachricht einstellen - Prüfung Existenz Task");
static constexpr Requirement A_20702_03 ("E-Rezept-Fachdienst - Task abrufen - Keine Einlöseinformationen in unbekannten Clients");
static constexpr Requirement A_19226_01 ("E-Rezept-Fachdienst - Task abrufen - Rückgabe Task inkl. Bundle im Bundle Apotheker");
static constexpr Requirement A_19569_03 ("E-Rezept-Fachdienst - Task abrufen - Versicherter - Suchparameter Task");
static constexpr Requirement A_18952    ( R"(E-Rezept-Fachdienst – Abfrage E-Rezept mit Status "gelöscht")");
static constexpr Requirement A_19030    ("E-Rezept-Fachdienst - unzulaessige Operationen Task");
static constexpr Requirement A_24176    ("E-Rezept-Fachdienst - Task abrufen - Apotheke - Verordnung abrufen - Prüfung Telematik-ID");
static constexpr Requirement A_24177    ("E-Rezept-Fachdienst - Task abrufen - Apotheke - Verordnung abrufen - Prüfung AccessCode");
static constexpr Requirement A_24178    ("E-Rezept-Fachdienst - Task abrufen - Apotheke - Verordnung abrufen - Prüfung Status in-progress");
static constexpr Requirement A_24179    ("E-Rezept-Fachdienst - Task abrufen - Apotheke - Verordnung abrufen - erneuter Abruf Verordnung");

// Requirements for endpoint POST /Task/$create
static constexpr Requirement A_19021_02 ("E-Rezept-Fachdienst - Task erzeugen - Generierung AccessCode");
static constexpr Requirement A_19112    ("E-Rezept-Fachdienst - Parametrierung Task fuer Workflow-Typ");
static constexpr Requirement A_19214    ("E-Rezept-Fachdienst - Ergaenzung Performer-Typ fuer Einloeseinstitutstyp");
static constexpr Requirement A_19445_10 ("FHIR FLOWTYPE für Prozessparameter");
static constexpr Requirement A_19114    ("E-Rezept-Fachdienst - Status draft");
static constexpr Requirement A_19257_01 ("E-Rezept-Fachdienst - Task erzeugen - Schemavalidierung Rezept anlegen");

// Requirements for endpoint POST /Task/{id}/$activate
static constexpr Requirement A_19024_03 ("E-Rezept-Fachdienst - Task aktivieren - Prüfung AccessCode Rezept aktualisieren");
static constexpr Requirement A_19128    ("E-Rezept-Fachdienst - Status aktivieren");
static constexpr Requirement A_19020    ("E-Rezept-Fachdienst - Schemavalidierung Rezept aktivieren");
static constexpr Requirement A_19025_03 ("E-Rezept-Fachdienst - Task aktivieren - QES prüfen Rezept aktualisieren");
static constexpr Requirement A_19029_06 ("E-Rezept-Fachdienst - Task aktivieren - Serversignatur Rezept aktivieren");
static constexpr Requirement A_19127_01 ("E-Rezept-Fachdienst - Task aktivieren - Übernahme der KVNR des Patienten");
static constexpr Requirement A_19225_02 ("E-Rezept-Fachdienst - Task aktivieren - Flowtype 160/169/200/209 - QES durch berechtigte Berufsgruppe");
static constexpr Requirement A_25990    ("E-Rezept-Fachdienst - Task aktivieren - Flowtype 162 - QES durch berechtigte Berufsgruppe");
static constexpr Requirement A_19999    ("E-Rezept-Fachdienst - Ergaenzung PerformerTyp fuer Einloeseinstitutstyp");
static constexpr Requirement A_20159    ("E-Rezept-Fachdienst - QES Pruefung Signaturzertifikat des HBA");
static constexpr Requirement A_19517_02 ("FHIR FLOWTYPE für Prozessparameter - Abweichende Festlegung für Entlassrezept");
static constexpr Requirement A_22222    ("E-Rezept-Fachdienst - Ausschluss weitere Kostenträger");
static constexpr Requirement A_22231    ("E-Rezept-Fachdienst - Ausschluss Betäubungsmittel und Thalidomid");
static constexpr Requirement A_22487    ("E-Rezept-Fachdienst - Task aktivieren - Prüfregel Ausstellungsdatum");
static constexpr Requirement A_22627_01 ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - zulässige Flowtype");
static constexpr Requirement A_22628    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Numerator-Denominator kleiner 5");
static constexpr Requirement A_22704    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Numerator größer 0");
static constexpr Requirement A_22629    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Denominator größer 1");
static constexpr Requirement A_22630    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Numerator kleiner / gleich Denominator");
static constexpr Requirement A_22631    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Unzulässige Angaben");
static constexpr Requirement A_22632    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - kein Entlassrezept");
static constexpr Requirement A_22633    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - keine Ersatzverordnung");
static constexpr Requirement A_22634    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Beginn Einlösefrist-Pflicht");
static constexpr Requirement A_23164    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Endedatum nicht vor Startdatum");
static constexpr Requirement A_23537    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Startdatum nicht vor Ausstellungsdatum");
static constexpr Requirement A_22927    ("E-Rezept-Fachdienst - Task aktivieren - Ausschluss unspezifizierter Extensions");
static constexpr Requirement A_23443_01 ("E-Rezept-Fachdienst – Task aktivieren – Flowtype 160/162/169 - Prüfung Coverage Type");
static constexpr Requirement A_23888    ("E-Rezept-Fachdienst - Task aktivieren - Überprüfung der IK Nummer im Profil KBV_PR_FOR_Coverage");
static constexpr Requirement A_24030    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der IK Nummer im Profil KBV_PR_FOR_Coverage");
static constexpr Requirement A_23890    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der KVNR Nummer im Profil KBV_PR_FOR_Patient");
static constexpr Requirement A_23891    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der ANR und ZANR im Profil KBV_PR_FOR_Practitioner");
static constexpr Requirement A_23892    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der PZN im Profil KBV_PR_ERP_Medication_PZN");
static constexpr Requirement A_24034    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der PZN im Profil KBV_PR_ERP_Medication_Compounding");
static constexpr Requirement A_24031    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der ANR und ZANR - Konfiguration bei Auffälligkeiten");
static constexpr Requirement A_24032    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der ANR und ZANR - Konfiguration Fehler");
static constexpr Requirement A_24033    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der ANR und ZANR - Konfiguration Warning");
static constexpr Requirement A_24901    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Schema ID");
static constexpr Requirement A_25991    ("E-Rezept-Fachdienst - Task aktivieren - Flowtype 162 - Prüfung Verordnung von DiGAs");
static constexpr Requirement A_26372    ("E-Rezept-Fachdienst – Task aktivieren – Flowtype 162 - Prüfung Coverage Alternative IK");
static constexpr Requirement A_25992    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der PZN im Profil KBV_PR_EVDGA_HealthAppRequest");
// PKV related:
static constexpr Requirement A_22347_01 ("E-Rezept-Fachdienst – Task aktivieren – Flowtype 200/209 - Prüfung Coverage Type");
static constexpr Requirement A_23936    ("E-Rezept-Fachdienst - Task aktivieren - Versicherten-ID als Identifikator von Versicherten");

// Requirements for endpoint POST /Task/$accept
static constexpr Requirement A_19149_02 ("E-Rezept-Fachdienst - Task akzeptieren - Prüfung Datensatz zwischenzeitlich gelöscht");
static constexpr Requirement A_19167_04 ("E-Rezept-Fachdienst - Task akzeptieren - Prüfung AccessCode");
static constexpr Requirement A_19168_01 ("E-Rezept-Fachdienst - Rezept bereits in Abgabe oder Bearbeitung");
static constexpr Requirement A_19169_01 ("E-Rezept-Fachdienst - Task akzeptieren - Generierung Secret, Statuswechsel in Abgabe und Rückgabewert");
static constexpr Requirement A_22635    ("E-Rezept-Fachdienst - Task akzeptieren - Mehrfachverordnung - Beginn Einlösefrist prüfen");
static constexpr Requirement A_23539_01 ("E-Rezept-Fachdienst - Task akzeptieren - Ende Einlösefrist prüfen");
static constexpr Requirement A_24174    ("E-Rezept-Fachdienst - Task akzeptieren - Telematik-ID der abgebenden LEI speichern");
// PKV related:
static constexpr Requirement A_22110    ("E-Rezept-Fachdienst – Task akzeptieren – Flowtype 200 - Einwilligung ermitteln");

// Requirements for endpoint POST /Task/$dispense
static constexpr Requirement A_24280    ("E-Rezept-Fachdienst - Dispensierinformationen bereitstellen - Prüfung Secret");
static constexpr Requirement A_24281_02 ("E-Rezept-Fachdienst - Dispensierinformationen bereitstellen - Schemaprüfung und Speicherung MedicationDispense");
static constexpr Requirement A_24298    ("E-Rezept-Fachdienst - Dispensierinformationen bereitstellen - Prüfung Status");
static constexpr Requirement A_24285_01 ("E-Rezept-Fachdienst - Dispensierinformationen bereitstellen - Zeitstempel");

// Requirements for endpoint POST /Task/$close
static constexpr Requirement A_19231_02 ("E-Rezept-Fachdienst - Task schliessen - Prüfung Secret");
static constexpr Requirement A_19248_05 ("E-Rezept-Fachdienst - Task schliessen - Schemaprüfung und Speicherung MedicationDispense");
static constexpr Requirement A_19232    ("E-Rezept-Fachdienst - Status beenden");
static constexpr Requirement A_20513    ("E-Rezept-Fachdienst - nicht mehr benötigte Einlösekommunikation");
static constexpr Requirement A_19233_05 ("E-Rezept-Fachdienst - Task schliessen - Quittung erstellen");
static constexpr Requirement A_22069    ("E-Rezept-Fachdienst - Task schliessen - Speicherung mehrerer MedicationDispenses");
static constexpr Requirement A_22070_02 ("E-Rezept-Fachdienst - MedicationDispense abrufen - Rückgabe mehrerer MedicationDispenses");
static constexpr Requirement A_23384_01 ("E-Rezept-Fachdienst - Prüfung Gültigkeit Profilversionen");
static constexpr Requirement A_24287    ("E-Rezept-Fachdienst - Task schließen - Aufruf ohne MedicationDispense");
static constexpr Requirement A_26337    ("E-Rezept-Fachdienst - Task schließen - Zeitstempel MedicationDispense");
static constexpr Requirement A_26002    ("E-Rezept-Fachdienst - Task schließen - Flowtype 160/169/200/209 - Profilprüfung MedicationDispense");
static constexpr Requirement A_26003    ("E-Rezept-Fachdienst - Task schließen - Flowtype 162 - Profilprüfung MedicationDispense");

// Requirements for endpoint POST /Task/$abort
static constexpr Requirement A_19145    ("E-Rezept-Fachdienst - Statusprüfung Apotheker löscht Rezept");
static constexpr Requirement A_19146    ("E-Rezept-Fachdienst - Statusprüfung Nutzer (außerhalb Apotheke) löscht Rezept");
static constexpr Requirement A_20546_03 ("E-Rezept-Fachdienst - E-Rezept löschen - Prüfung KVNR, Versicherter löscht Rezept");
static constexpr Requirement A_20547    ("E-Rezept-Fachdienst - Prüfung KVNR, Vertreter löscht Rezept");
static constexpr Requirement A_19120_3  ("E-Rezept-Fachdienst - Prüfung AccessCode, Verordnender löscht Rezept");
static constexpr Requirement A_19224    ("E-Rezept-Fachdienst - Prüfung Secret, Apotheker löscht Rezept");
static constexpr Requirement A_19027_06 ("E-Rezept-Fachdienst - E-Rezept löschen - Medizinische und personenbezogene Daten löschen");
static constexpr Requirement A_19121    ("E-Rezept-Fachdienst - Finaler Status cancelled");

// Requirements for endpoint POST /Task/$reject
static constexpr Requirement A_19171_03 ("E-Rezept-Fachdienst - Task zurückweisen - Prüfung Secret");
static constexpr Requirement A_19172_01 ("E-Rezept-Fachdienst - Task zurückweisen - Secret löschen und Status setzen");
static constexpr Requirement A_24175    ("E-Rezept-Fachdienst - Task zurückweisen - Telematik-ID der abgebenden LEI löschen");
static constexpr Requirement A_24286_02 ("E-Rezept-Fachdienst - Task zurückweisen - Dispensierinformationen löschen");

// Requirements for endpoint /AuditEvent and /AuditEvent/<id>
static constexpr Requirement A_19402   ("E-Rezept-Fachdienst - unzulässige Operationen AuditEvent");
static constexpr Requirement A_19396   ("E-Rezept-Fachdienst - Filter AuditEvent auf KVNR des Versicherten");
static constexpr Requirement A_19399   ("E-Rezept-Fachdienst - Suchparameter AuditEvent");
static constexpr Requirement A_19397   ("E-Rezept-Fachdienst - Rückgabe AuditEvents im Bundle");
static constexpr Requirement A_19398   ("E-Rezept-Fachdienst - Rückgabe AuditEvents im Bundle Paging");

// Requirements for endpoint /MedicationDispense
static constexpr Requirement A_19400   ("E-Rezept-Fachdienst - unzulaessige Operationen MedicationDispense");
static constexpr Requirement A_19406_01("E-Rezept-Fachdienst - MedicationDispense abrufen - Filter MedicationDispense auf KVNR des Versicherten");
static constexpr Requirement A_19518   ("E-Rezept-Fachdienst - Suchparameter für MedicationDispense");
static constexpr Requirement A_19140   ("Logische Operation - Dispensierinformationen durch Versicherten abrufen");
static constexpr Requirement A_19141   ("Logische Operation - Dispensierinformation für ein einzelnes E-Rezept abrufen");
static constexpr Requirement A_24443   ("E-Rezept-Fachdienst - Handhabung der Rückgabe von mehreren FHIR-Objekten - Paginierung");
static constexpr Requirement A_26527   ("E-Rezept-Fachdienst - MedicationDispense abrufen -Referenzierung MedicationDispense und Medication");
// Requirements for endpoint /ChargeItem and /ChargeItem/<id>
static constexpr Requirement A_22111   ("E-Rezept-Fachdienst - ChargeItem - unzulässige Operationen");
// POST /ChargeItem
static constexpr Requirement A_22130   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen - Prüfung Parameter Task");
static constexpr Requirement A_22131   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen - Prüfung Existenz Task");
static constexpr Requirement A_22132_02("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Prüfung Secret Task");
static constexpr Requirement A_22133   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Prüfung Einwilligung");
static constexpr Requirement A_22134   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Verordnungsdatensatz übernehmen");
static constexpr Requirement A_22135_01("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Quittung übernehmen");
static constexpr Requirement A_22136_01("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – FHIR-Validierung ChargeItem");
static constexpr Requirement A_22137   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Abgabedatensatz übernehmen");
static constexpr Requirement A_22138   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Validierung PKV-Abgabedatensatz");
static constexpr Requirement A_22139   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Signaturprüfung PKV-Abgabedatensatz");
static constexpr Requirement A_22140_01("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Prüfung Signaturzertifikat PKV-Abgabedatensatz");
static constexpr Requirement A_22141   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Signaturzertifikat SMC-B prüfen");
static constexpr Requirement A_22143   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – ChargeItem befüllen");
static constexpr Requirement A_22614_02("E-Rezept-Fachdienst - Abrechnungsinformation bereitstellen - Generierung AccessCode");
static constexpr Requirement A_23704   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – kein AccessCode und Quittung");
static constexpr Requirement A_24471   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – ChargeItem-ID=Rezept-ID");
// PUT /ChargeItem
static constexpr Requirement A_22215   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern – Prüfung Einwilligung");
static constexpr Requirement A_22146   ("E-Rezept-Fachdienst – Abrechnungsinformationen ändern – Apotheke - Prüfung Telematik-ID");
static constexpr Requirement A_22148   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern – Apotheke – PKV-Abgabedatensatz übernehmen");
static constexpr Requirement A_22149   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern – Apotheke – FHIR-Validierung PKV-Abgabedatensatz");
static constexpr Requirement A_22150   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern - Apotheke – Signaturprüfung PKV-Abgabedatensatz");
static constexpr Requirement A_22151_01("E-Rezept-Fachdienst – Abrechnungsinformation ändern – Apotheke – Prüfung Signaturzertifikat PKV-Abgabedatensatz");
static constexpr Requirement A_22152   ("E-Rezept-Fachdienst - Abrechnungsinformation ändern – FHIR-Validierung ChargeItem");
static constexpr Requirement A_22615_02("E-Rezept-Fachdienst - Abrechnungsinformation ändern - Apotheke - Generierung AccessCode");
static constexpr Requirement A_22616_03("E-Rezept-Fachdienst - Abrechnungsinformation ändern - Apotheke - Prüfung AccessCode");
static constexpr Requirement A_23624   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern – Apotheke – kein AccessCode und Quittung");
// DELETE /ChargeItem
static constexpr Requirement A_22112   ("E-Rezept-Fachdienst - Abrechnungsinformation löschen - alles Löschen verbieten");
static constexpr Requirement A_22114   ("E-Rezept-Fachdienst - Abrechnungsinformation löschen – Prüfung KVNR");
static constexpr Requirement A_22115   ("E-Rezept-Fachdienst - Abrechnungsinformation löschen – Prüfung Telematik-ID");
static constexpr Requirement A_22117_01("E-Rezept-Fachdienst - Abrechnungsinformation löschen - zu löschende Ressourcen");
// GET /ChargeItem
static constexpr Requirement A_22119   ("E-Rezept-Fachdienst – Abrechnungsinformationen lesen – Versicherter – Filter KVNR");
static constexpr Requirement A_22121   ("E-Rezept-Fachdienst – Abrechnungsinformationen lesen - Suchkriterien");
static constexpr Requirement A_22122   ("E-Rezept-Fachdienst – Abrechnungsinformationen lesen – Response");
static constexpr Requirement A_22123   ("E-Rezept-Fachdienst – Abrechnungsinformationen lesen - Paging");
static constexpr Requirement A_22611_02("E-Rezept-Fachdienst - Abrechnungsinformation abrufen - Apotheke - Prüfung AccessCode");
// Requirements for endpoint /ChargeItem/<id>
static constexpr Requirement A_22125   ("E-Rezept-Fachdienst - Abrechnungsinformation lesen - Versicherter - Prüfung KVNR");
static constexpr Requirement A_22126   ("E-Rezept-Fachdienst - Abrechnungsinformation lesen - Apotheke - Prüfung Telematik-ID");
static constexpr Requirement A_22127   ("E-Rezept-Fachdienst - Abrechnungsinformation lesen - Versicherte - Signieren");
static constexpr Requirement A_22128_01("E-Rezept-Fachdienst - Abrechnungsinformation lesen - Apotheke - kein AccessCode und Quittung");
// PATCH /ChargeItem/<id>
static constexpr Requirement A_22875   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern (PATCH) – Rollenprüfung");
static constexpr Requirement A_22877   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern (PATCH) – Versicherter - Prüfung KVNR");
static constexpr Requirement A_22878   ("E-Rezept-Fachdienst - Abrechnungsinformation ändern (PATCH) – Zulässige Felder");
static constexpr Requirement A_22879   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern (PATCH) - alles Ändern verbieten");

// Requirements for endpoint /Consent and /Consent/<id>
static constexpr Requirement A_22153   ("E-Rezept-Fachdienst - unzulässige Operationen Consent");
static constexpr Requirement A_22154   (" E-Rezept-Fachdienst - Consent loeschen - alles Loeschen verbieten");
static constexpr Requirement A_22156   ("E-Rezept-Fachdienst - Consent löschen – Prüfung KVNR");
static constexpr Requirement A_22157   ("E-Rezept-Fachdienst - Consent loeschen - Loeschen der bestehenden Abrechnungsinformationen");
static constexpr Requirement A_22158   ("E-Rezept-Fachdienst - Consent loeschen - Loeschen der Consent");
static constexpr Requirement A_22160   ("E-Rezept-Fachdienst - Consent lesen - Filter Consent auf KVNR des Versicherten");
static constexpr Requirement A_22162   ("E-Rezept-Fachdienst - Consent schreiben – nur eine Einwilligung CHARGCONS pro KVNR");
static constexpr Requirement A_22289   ("E-Rezept-Fachdienst - Consent schreiben - Prüfung KVNR");
static constexpr Requirement A_22350   ("E-Rezept-Fachdienst - Consent schreiben - Persistieren");
static constexpr Requirement A_22351   ("E-Rezept-Fachdienst - Consent schreiben - FHIR-Validierung");
static constexpr Requirement A_22874_01("E-Rezept-Fachdienst - Consent löschen - Prüfung category");

// Requirements for ACCESS_TOKEN checks
static constexpr Requirement A_20362    ("E-Rezept-Fachdienst - 'ACCESS_TOKEN' generelle Struktur");
static constexpr Requirement A_20365_01 ("E-Rezept-Fachdienst - Die Signatur des ACCESS_TOKEN ist zu prüfen");
static constexpr Requirement A_20369_01 ("E-Rezept-Fachdienst - Abbruch bei unerwarteten Inhalten");
static constexpr Requirement A_20370    ("E-Rezept-Fachdienst - Abbruch bei falschen Datentypen der Attribute");
static constexpr Requirement A_20373    ("E-Rezept-Fachdienst - Prüfung der Gültigkeit des ACCESS_TOKEN für den Zugriff auf Fachdienste ohne nbf");
static constexpr Requirement A_20374    ("E-Rezept-Fachdienst - Prüfung der Gültigkeit des ACCESS_TOKEN für den Zugriff auf Fachdienste mit nbf");
static constexpr Requirement A_20504    ("E-Rezept-Fachdienst - Reaktion bei ungültiger oder fehlender Signatur des ACCESS_TOKEN");
static constexpr Requirement A_20974    ("E-Rezept-Fachdienst - Prüfungsintervall Signaturzertifikat E-Rezept-Fachdienst");
static constexpr Requirement A_20158_01 ("E-Rezept-Fachdienst - Pruefung Signaturzertifikat IdP");
static constexpr Requirement A_21520    ("E-Rezept-Fachdienst - Prüfung des aud Claim des ACCESS_TOKEN mit der vom Fachdienst registrierten URI");

// Transactions
static constexpr Requirement A_18936 ("E-Rezept-Fachdienst - Transaktionssicherheit");

// TSL/QES related requirements
static constexpr Requirement GS_A_4898     ("PKI - TSL-Grace-Period einer TSL");
static constexpr Requirement GS_A_4899     ("PKI - TSL Update-Prüfintervall");
static constexpr Requirement GS_A_5336     ("PKI - Zertifikatspruefung nach Ablauf TSL-Graceperiod");
static constexpr Requirement GS_A_4661_01  ("PKI - kritische Erweiterungen in Zertifikaten");

// C_10854: E-Rezept: Versionierung FHIR-Ressourcen
static constexpr Requirement A_22216 ("FHIR-Ressourcen Versionsangabe");

// Workflow 169 / 209
static constexpr Requirement A_21360_01 ("E-Rezept-Fachdienst - Task abrufen - Flowtype 169 / 209 - keine Einlöseinformationen");
static constexpr Requirement A_21370 ("E-Rezept-Fachdienst - Prüfung Rezept-ID und Präfix gegen Flowtype");
static constexpr Requirement A_22102_01 ("E-Rezept-Fachdienst - E-Rezept löschen - Flowtype 169 / 209 - Versicherter - Statusprüfung");
static constexpr Requirement A_26148 ("E-Rezept-Fachdienst - Task abrufen - Flowtype 169/209 - Nicht verfügbar bei KVNR-Mismatch");

// Requirements for endpoint /Subscription
static constexpr Requirement A_22362_01 ("E-Rezept-Fachdienst - Rollenpruefung Apotheke 'Create Subscription'");
static constexpr Requirement A_22363    ("E-Rezept-Fachdienst - Create Subscription 'validate telematic id'");
static constexpr Requirement A_22364    ("E-Rezept-Fachdienst - Create Subscription 'POST /Subscription response'");
static constexpr Requirement A_22365    ("E-Rezept-Fachdienst - Register Subscription 'pseudonym for telematic id'");
static constexpr Requirement A_22366    ("E-Rezept-Fachdienst - Create Subscription 'Bearer Token'");
static constexpr Requirement A_22367_02 ("E-Rezept-Fachdienst - Nachricht einstellen - Notification Apotheke");
static constexpr Requirement A_22383    ("E-Rezept-Fachdienst – Generierungsschlüssel – Pseudonym der Telematik-ID");

static constexpr Requirement A_22698    ("E-Rezept-Fachdienst - Erzeugung des Nutzerpseudonyms LEI");
static constexpr Requirement A_22975    ("Performance - Rohdaten-Performance-Bericht - Konfigurationpseudonymisierte Werte der Telematik-ID");
static constexpr Requirement A_23090_02 ("Performance - Rohdaten - Spezifika E-Rezept – Message (Rohdatenerfassung v.02)");

// Requirements for VSDM++
static constexpr Requirement A_23450    ("E-Rezept-Fachdienst - Rezepte lesen - Apotheke - VSDM - Prüfung Prüfungsnachweis");
static constexpr Requirement A_23451_01 ("E-Rezept-Fachdienst - Rezepte lesen - Apotheke - VSDM - Zeitraum Akzeptanz Prüfungsnachweis");
static constexpr Requirement A_23452_02 ("E-Rezept-Fachdienst - Rezepte lesen - Apotheke - VSDM - Filter KVNR");
static constexpr Requirement A_23454    ("E-Rezept-Fachdienst - Prüfung Prüfziffer");
static constexpr Requirement A_23456_01 ("E-Rezept-Fachdienst - Prüfung Prüfziffer - Berechnung HMAC der Prüfziffer");
static constexpr Requirement A_23492    ("E-Rezept-Fachdienst - VSDM HMAC-Schlüssel - Exportpaket einbringen");
static constexpr Requirement A_23493    ("E-Rezept-Fachdienst - VSDM HMAC-Schlüssel - Prüfung");
static constexpr Requirement A_23486    ("E-Rezept-Fachdienst - VSDM HMAC-Schlüssel - Ausgabe");
static constexpr Requirement A_25200    ("E-Rezept-Fachdienst - Rezepte lesen - Apotheke - VSDM - PN3 - Status");
static constexpr Requirement A_25206    ("E-Rezept-Fachdienst - Rezepte lesen - Apotheke - VSDM - PN3");
static constexpr Requirement A_25207    ("E-Rezept-Fachdienst - Rezepte lesen - Apotheke - VSDM - PN3 - AcceptPN3 false");
static constexpr Requirement A_25208_01 ("E-Rezept-Fachdienst - Rezepte lesen - Apotheke - VSDM - URL kvnr");
static constexpr Requirement A_25209    ("E-Rezept-Fachdienst - Rezepte lesen - Apotheke - VSDM - PN3 - AcceptPN3 true - Filter Status, KVNR und Workflowtype");
static constexpr Requirement A_27346    ("E-Rezept-Fachdienst - Rezepte lesen - Apotheke - VSDM - URL hcv");
static constexpr Requirement A_27445    ("E-Rezept-Fachdienst - Rezepte lesen - Apotheke - Ratelimit pro Telematik-ID pro Tag");
static constexpr Requirement A_27446    ("E-Rezept-Fachdienst - Rezepte lesen - Apotheke - Ratelimit pro Telematik-ID prüfen");

// Default sort for GET handlers
static constexpr Requirement A_24438    ("E-Rezept-Fachdienst - Handhabung der Rückgabe von mehreren FHIR-Objekten - Sortieren von Einträgen");

static constexpr Requirement A_19296_03("E-Rezept-Fachdienst - Inhalt Protokolleintrag");
static constexpr Requirement A_25962 ("E-Rezept-Fachdienst - ePA - Medication Service - Versichertenprotokoll ");

static constexpr Requirement A_25165_03 ("Authorization Service: JWT Bearer-Token des E-Rezept-Fachdienstes");

#endif

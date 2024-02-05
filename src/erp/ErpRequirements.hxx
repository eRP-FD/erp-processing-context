/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
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
static Requirement A_21558_01 ("E-Rezept-Fachdienst - Rollenprüfung Versicherter liest Rezepte");
static Requirement A_19166    ("E-Rezept-Fachdienst - Rollenpruefung Abgebender ruft Rezept ab");
static Requirement A_19170_01 ("E-Rezept-Fachdienst - Rollenprüfung Abgebender weist zurück");
static Requirement A_19230    ("E-Rezept-Fachdienst - Rollenpruefung Abgebender vollzieht Abgabe des Rezepts");
static Requirement A_19389    ("E-Rezept-Fachdienst - Authentifizierung erforderlich Vers-Endpunkt");
static Requirement A_19390    ("E-Rezept-Fachdienst - Authentifizierung Nutzerrolle");
static Requirement A_19391_01 ("E-Rezept-Fachdienst - Authentifizierung Nutzername");
static Requirement A_19392    ("E-Rezept-Fachdienst - Authentifizierung Nutzerkennung");
static Requirement A_19439_02 ("E-Rezept-Fachdienst - Authentifizierung Authentifizierungsstärke");
static Requirement A_19395    ("E-Rezept-Fachdienst - Rollenpruefung Versicherter liest AuditEvent");
static Requirement A_19405    ("E-Rezept-Fachdienst - Rollenpruefung Versicherter liest MedicationDispense");
static Requirement A_19446_01 ("E-Rezept-Fachdienst - Rollenpruefung Versicherter oder Apotheker liest Rezept");
// PKV related Rollenprüfung:
static Requirement A_22113    ("E-Rezept-Fachdienst - Abrechnungsinformation löschen - Rollenprüfung");
static Requirement A_22118    ("E-Rezept-Fachdienst - Abrechnungsinformationen lesen - Rollenprüfung Versicherter oder Apotheker");
static Requirement A_22124    ("E-Rezept-Fachdienst - Abrechnungsinformationen lesen - Rollenprüfung Versicherter oder Apotheker");
static Requirement A_22129    ("E-Rezept-Fachdienst - Abrechnungsinformation bereitstellen - Rollenprüfung");
static Requirement A_22144    ("E-Rezept-Fachdienst - Abrechnungsinformation ändern – Rollenprüfung");
static Requirement A_22155    ("E-Rezept-Fachdienst - Consent loeschen - Rollenprüfung Versicherter");
static Requirement A_22159    ("E-Rezept-Fachdienst - Consent lesen - Rollenprüfung Versicherter");
static Requirement A_22161    ("E-Rezept-Fachdienst - Consent schreiben - Rollenprüfung Versicherter");

// Requirements for Communications
static Requirement A_19401    ("E-Rezept-Fachdienst - unzulässige Operationen Communication");
static Requirement A_19447_04 ("E-Rezept-Fachdienst - Nachricht einstellen - Schemaprüfung");
static Requirement A_19448_01 ("E-Rezept-Fachdienst - Nachricht einstellen - Absender und Sendedatum");
static Requirement A_19450_01 ("E-Rezept-Fachdienst - Nachricht einstellen Schadcodeprüfung");
static Requirement A_19520_01 ("E-Rezept-Fachdienst - Nachrichten abrufen - für Empfänger filtern");
static Requirement A_19521    ("E-Rezept-Fachdienst - Nachrichten als abgerufen markieren");
static Requirement A_19522    ("E-Rezept-Fachdienst - Nachrichtenabruf Suchparameter");
static Requirement A_19534    ("E-Rezept-Fachdienst - Rückgabe Communication im Bundle Paging");
static Requirement A_20229_01 ("E-Rezept-Fachdienst - Nachricht einstellen - Nachrichtenzähler bei Versicherter-zu-Versichertem-Kommunikation");
static Requirement A_20230_01 ("E-Rezept-Fachdienst - Nachricht einstellen - Einlösbare E-Rezepte für Versicherter-zu-Versichertem-Kommunikation");
static Requirement A_20231    ("E-Rezept-Fachdienst - Ausschluss Nachrichten an Empfänger gleich Absender");
static Requirement A_20258    ("E-Rezept-Fachdienst - Communication löschen auf Basis Absender-ID");
static Requirement A_20259    ("E-Rezept-Fachdienst - Communication löschen mit Warnung wenn vom Empfänger bereits abgerufen");
static Requirement A_20260    (R"(Anwendungsfall "Nachricht durch Versicherten löschen")");
static Requirement A_20776    (R"(Anwendungsfall "Nachricht durch Abgebenden löschen")");
static Requirement A_20885_03 ("E-Rezept-Fachdienst - Nachricht einstellen - Versicherte - Prüfung Versichertenbezug und Berechtigung");
static Requirement A_22731    ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Prüfung Flowtype Task");
static Requirement A_22734    ("E-Rezept-Fachdienst - Nachricht einstellen - Prüfung Existenz ChargeItem");
static Requirement A_23878    ("E-Rezept-Fachdienst - Nachricht einstellen - Validierung des Payload-Inhalt von GEM_ERP_PR_Communication_DispReq");
static Requirement A_23879    ("E-Rezept-Fachdienst - Nachricht einstellen - Validierung des Payload-Inhalt von GEM_ERP_PR_Communication_Reply");

// Requirements for "Claims der Identitätsbestätigung"
static Requirement A_19130    ("E-Rezept-Fachdienst - Authentifizierung erforderlich LEI-Endpunkt");
static Requirement A_19131    ("E-Rezept-Fachdienst - Authentifizierung ungültig");
static Requirement A_19132    ("E-Rezept-Fachdienst - Authentifizierung Signaturprüfung");
static Requirement A_19189    ("E-Rezept-Fachdienst - Authentifizierung erforderlich Vers-Endpunkt");
static Requirement A_19992    ("E-Rezept-Fachdienst - Blocklisting zu häufig verwendeter ACCESS_TOKEN");
static Requirement A_19993_01 ("E-Rezept-Fachdienst - Prüfung eingehender ACCESS_TOKEN");

// Requirements for TEE (VAU) protocol.
static Requirement A_20160    ("E-Rezept-VAU, Schlüsselpaar und Zertifikat");
static Requirement A_20161_01 ("E-Rezept-Client, Request-Erstellung");
static Requirement A_20162    ("E-Rezept-FD, Webschnittstellen, VAU-Requests");
static Requirement A_20163    ("E-Rezept-VAU, Nutzeranfrage, Ent-und Verschlüsselung");
static Requirement A_20174    ("E-Rezept-Client, Response-Auswertung");
static Requirement A_20175    ("E-Rezept-Client, Speicherung  Nutzerpseudonym");

// Requirements for Prescription ID (Rezept-ID)
static Requirement A_19217_01 ("Aufbau E-Rezept-ID");
static Requirement A_19218    ("Prüfung E-Rezept-ID");
static Requirement A_19019_01 ("E-Rezept-Fachdienst - Task erzeugen - Generierung Rezept-ID");

// Requirements for endpoints GET /Task and GET /Task/{id}
static Requirement A_19115_01 ("E-Rezept-Fachdienst - Task abrufen - Filter Tasks auf KVNR des Versicherten");
static Requirement A_19116_01 ("E-Rezept-Fachdienst - Task abrufen - Prüfung AccessCode bei KVNR-Missmatch");
static Requirement A_19129_01 ("E-Rezept-Fachdienst - Rückgabe Tasks im Bundle Versicherter");
static Requirement A_21375_02 ("E-Rezept-Fachdienst - Task abrufen - Rückgabe Task inkl. Bundles Versicherter");
static Requirement A_21532_02 ("E-Rezept-Fachdienst - Task abrufen - Kein Secret für Versicherte");
static Requirement A_21371_02 ("E-Rezept-Fachdienst - Nachricht einstellen - Prüfung Existenz Task");
static Requirement A_20702_03 ("E-Rezept-Fachdienst - Task abrufen - Keine Einlöseinformationen in unbekannten Clients");
static Requirement A_19226_01 ("E-Rezept-Fachdienst - Task abrufen - Rückgabe Task inkl. Bundle im Bundle Apotheker");
static Requirement A_19569_03 ("E-Rezept-Fachdienst - Task abrufen - Versicherter - Suchparameter Task");
static Requirement A_18952    ( R"(E-Rezept-Fachdienst – Abfrage E-Rezept mit Status "gelöscht")");
static Requirement A_19030    ("E-Rezept-Fachdienst - unzulaessige Operationen Task");
static Requirement A_24176    ("E-Rezept-Fachdienst - Task abrufen - Apotheke - Verordnung abrufen - Prüfung Telematik-ID");
static Requirement A_24177    ("E-Rezept-Fachdienst - Task abrufen - Apotheke - Verordnung abrufen - Prüfung AccessCode");
static Requirement A_24178    ("E-Rezept-Fachdienst - Task abrufen - Apotheke - Verordnung abrufen - Prüfung Status in-progress");
static Requirement A_24179    ("E-Rezept-Fachdienst - Task abrufen - Apotheke - Verordnung abrufen - erneuter Abruf Verordnung");

// Requirements for endpoint POST /Task/$create
static Requirement A_19021_02 ("E-Rezept-Fachdienst - Task erzeugen - Generierung AccessCode");
static Requirement A_19112    ("E-Rezept-Fachdienst - Parametrierung Task fuer Workflow-Typ");
static Requirement A_19214    ("E-Rezept-Fachdienst - Ergaenzung Performer-Typ fuer Einloeseinstitutstyp");
static Requirement A_19445_08 ("FHIR FLOWTYPE für Prozessparameter");
static Requirement A_19114    ("E-Rezept-Fachdienst - Status draft");
static Requirement A_19257_01 ("E-Rezept-Fachdienst - Task erzeugen - Schemavalidierung Rezept anlegen");

// Requirements for endpoint POST /Task/{id}/$activate
static Requirement A_19024_03 ("E-Rezept-Fachdienst - Task aktivieren - Prüfung AccessCode Rezept aktualisieren");
static Requirement A_19128    ("E-Rezept-Fachdienst - Status aktivieren");
static Requirement A_19020    ("E-Rezept-Fachdienst - Schemavalidierung Rezept aktivieren");
static Requirement A_19025_02 ("E-Rezept-Fachdienst - Task aktivieren - QES prüfen Rezept aktualisieren");
static Requirement A_19029_05 ("E-Rezept-Fachdienst - Task aktivieren - Serversignatur Rezept aktivieren");
static Requirement A_19127_01 ("E-Rezept-Fachdienst - Task aktivieren - Übernahme der KVNR des Patienten");
static Requirement A_19225    ("E-Rezept-Fachdienst - QES durch berechtigte Berufsgruppe");
static Requirement A_19999    ("E-Rezept-Fachdienst - Ergaenzung PerformerTyp fuer Einloeseinstitutstyp");
static Requirement A_20159    ("E-Rezept-Fachdienst - QES Pruefung Signaturzertifikat des HBA");
static Requirement A_19517_02 ("FHIR FLOWTYPE für Prozessparameter - Abweichende Festlegung für Entlassrezept");
static Requirement A_22222    ("E-Rezept-Fachdienst - Ausschluss weitere Kostenträger");
static Requirement A_22231    ("E-Rezept-Fachdienst - Ausschluss Betäubungsmittel und Thalidomid");
static Requirement A_22487    ("E-Rezept-Fachdienst - Task aktivieren - Prüfregel Ausstellungsdatum");
static Requirement A_22627_01 ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - zulässige Flowtype");
static Requirement A_22628    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Numerator-Denominator kleiner 5");
static Requirement A_22704    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Numerator größer 0");
static Requirement A_22629    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Denominator größer 1");
static Requirement A_22630    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Numerator kleiner / gleich Denominator");
static Requirement A_22631    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Unzulässige Angaben");
static Requirement A_22632    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - kein Entlassrezept");
static Requirement A_22633    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - keine Ersatzverordnung");
static Requirement A_22634    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Beginn Einlösefrist-Pflicht");
static Requirement A_23164    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Endedatum nicht vor Startdatum");
static Requirement A_23537    ("E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Startdatum nicht vor Ausstellungsdatum");
static Requirement A_22927    ("E-Rezept-Fachdienst - Task aktivieren - Ausschluss unspezifizierter Extensions");
static Requirement A_23443    ("E-Rezept-Fachdienst – Task aktivieren – Flowtype 160/169 - Prüfung Coverage Type");
static Requirement A_23888    ("E-Rezept-Fachdienst - Task aktivieren - Überprüfung der IK Nummer im Profil KBV_PR_FOR_Coverage");
static Requirement A_24030    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der IK Nummer im Profil KBV_PR_FOR_Coverage");
static Requirement A_23890    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der KVNR Nummer im Profil KBV_PR_FOR_Patient");
static Requirement A_23891    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der ANR und ZANR im Profil KBV_PR_FOR_Practitioner");
static Requirement A_23892    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der PZN im Profil KBV_PR_ERP_Medication_PZN");
static Requirement A_24034    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der PZN im Profil KBV_PR_ERP_Medication_Compounding");
static Requirement A_24031    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der ANR und ZANR - Konfiguration bei Auffälligkeiten");
static Requirement A_24032    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der ANR und ZANR - Konfiguration Fehler");
static Requirement A_24033    ("E-Rezept-Fachdienst - Task aktivieren – Überprüfung der ANR und ZANR - Konfiguration Warning");
// PKV related:
static Requirement A_22347_01 ("E-Rezept-Fachdienst – Task aktivieren – Flowtype 200/209 - Prüfung Coverage Type");
static Requirement A_23936    ("E-Rezept-Fachdienst - Task aktivieren - Versicherten-ID als Identifikator von Versicherten");

// Requirements for endpoint POST /Task/$accept
static Requirement A_19149_02 ("E-Rezept-Fachdienst - Task akzeptieren - Prüfung Datensatz zwischenzeitlich gelöscht");
static Requirement A_19167_04 ("E-Rezept-Fachdienst - Task akzeptieren - Prüfung AccessCode");
static Requirement A_19168_01 ("E-Rezept-Fachdienst - Rezept bereits in Abgabe oder Bearbeitung");
static Requirement A_19169_01 ("E-Rezept-Fachdienst - Task akzeptieren - Generierung Secret, Statuswechsel in Abgabe und Rückgabewert");
static Requirement A_22635    ("E-Rezept-Fachdienst - Task akzeptieren - Mehrfachverordnung - Beginn Einlösefrist prüfen");
static Requirement A_23539_01 ("E-Rezept-Fachdienst - Task akzeptieren - Ende Einlösefrist prüfen");
static Requirement A_24174    ("E-Rezept-Fachdienst - Task akzeptieren - Telematik-ID der abgebenden LEI speichern");
// PKV related:
static Requirement A_22110    ("E-Rezept-Fachdienst – Task akzeptieren – Flowtype 200 - Einwilligung ermitteln");

// Requirements for endpoint POST /Task/$close
static Requirement A_19231_02 ("E-Rezept-Fachdienst - Task schliessen - Prüfung Secret");
static Requirement A_19248_02 ("E-Rezept-Fachdienst - Task schliessen - Schemaprüfung und Speicherung MedicationDispense");
static Requirement A_19232    ("E-Rezept-Fachdienst - Status beenden");
static Requirement A_20513    ("E-Rezept-Fachdienst - nicht mehr benötigte Einlösekommunikation");
static Requirement A_19233_05 ("E-Rezept-Fachdienst - Task schliessen - Quittung erstellen");
static Requirement A_22069    ("E-Rezept-Fachdienst - Task schliessen - Speicherung mehrerer MedicationDispenses");
static Requirement A_22070_02 ("E-Rezept-Fachdienst - MedicationDispense abrufen - Rückgabe mehrerer MedicationDispenses");
static Requirement A_23384    ("E-Rezept-Fachdienst - Prüfung Gültigkeit Profilversionen");

// Requirements for endpoint POST /Task/$abort
static Requirement A_19145    ("E-Rezept-Fachdienst - Statusprüfung Apotheker löscht Rezept");
static Requirement A_19146    ("E-Rezept-Fachdienst - Statusprüfung Nutzer (außerhalb Apotheke) löscht Rezept");
static Requirement A_20546_03 ("E-Rezept-Fachdienst - E-Rezept löschen - Prüfung KVNR, Versicherter löscht Rezept");
static Requirement A_20547    ("E-Rezept-Fachdienst - Prüfung KVNR, Vertreter löscht Rezept");
static Requirement A_19120_3  ("E-Rezept-Fachdienst - Prüfung AccessCode, Verordnender löscht Rezept");
static Requirement A_19224    ("E-Rezept-Fachdienst - Prüfung Secret, Apotheker löscht Rezept");
static Requirement A_19027_04 ("E-Rezept-Fachdienst - E-Rezept löschen - Medizinische und personenbezogene Daten löschen");
static Requirement A_19121    ("E-Rezept-Fachdienst - Finaler Status cancelled");

// Requirements for endpoint POST /Task/$reject
static Requirement A_19171_03 ("E-Rezept-Fachdienst - Task zurückweisen - Prüfung Secret");
static Requirement A_19172_01 ("E-Rezept-Fachdienst - Task zurückweisen - Secret löschen und Status setzen");
static Requirement A_24175    ("E-Rezept-Fachdienst - Task zurückweisen - Telematik-ID der abgebenden LEI löschen");

// Requirements for endpoint /AuditEvent and /AuditEvent/<id>
static Requirement A_19402   ("E-Rezept-Fachdienst - unzulässige Operationen AuditEvent");
static Requirement A_19396   ("E-Rezept-Fachdienst - Filter AuditEvent auf KVNR des Versicherten");
static Requirement A_19399   ("E-Rezept-Fachdienst - Suchparameter AuditEvent");
static Requirement A_19397   ("E-Rezept-Fachdienst - Rückgabe AuditEvents im Bundle");
static Requirement A_19398   ("E-Rezept-Fachdienst - Rückgabe AuditEvents im Bundle Paging");

// Requirements for endpoint /MedicationDispense
static Requirement A_19400   ("E-Rezept-Fachdienst - unzulaessige Operationen MedicationDispense");
static Requirement A_19406_01("E-Rezept-Fachdienst - MedicationDispense abrufen - Filter MedicationDispense auf KVNR des Versicherten");
static Requirement A_19518   ("E-Rezept-Fachdienst - Suchparameter für MedicationDispense");
static Requirement A_19140   ("Logische Operation - Dispensierinformationen durch Versicherten abrufen");
static Requirement A_19141   ("Logische Operation - Dispensierinformation für ein einzelnes E-Rezept abrufen");

// Requirements for endpoint /ChargeItem and /ChargeItem/<id>
static Requirement A_22111   ("E-Rezept-Fachdienst - ChargeItem - unzulässige Operationen");
// POST /ChargeItem
static Requirement A_22130   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen - Prüfung Parameter Task");
static Requirement A_22131   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen - Prüfung Existenz Task");
static Requirement A_22132_02("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Prüfung Secret Task");
static Requirement A_22133   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Prüfung Einwilligung");
static Requirement A_22134   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Verordnungsdatensatz übernehmen");
static Requirement A_22135_01("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Quittung übernehmen");
static Requirement A_22136_01("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – FHIR-Validierung ChargeItem");
static Requirement A_22137   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Abgabedatensatz übernehmen");
static Requirement A_22138   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Validierung PKV-Abgabedatensatz");
static Requirement A_22139   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Signaturprüfung PKV-Abgabedatensatz");
static Requirement A_22140_01("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Prüfung Signaturzertifikat PKV-Abgabedatensatz");
static Requirement A_22141   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – Signaturzertifikat SMC-B prüfen");
static Requirement A_22143   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – ChargeItem befüllen");
static Requirement A_22614_02("E-Rezept-Fachdienst - Abrechnungsinformation bereitstellen - Generierung AccessCode");
static Requirement A_23704   ("E-Rezept-Fachdienst – Abrechnungsinformation bereitstellen – kein AccessCode und Quittung");
// PUT /ChargeItem
static Requirement A_22215   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern – Prüfung Einwilligung");
static Requirement A_22146   ("E-Rezept-Fachdienst – Abrechnungsinformationen ändern – Apotheke - Prüfung Telematik-ID");
static Requirement A_22148   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern – Apotheke – PKV-Abgabedatensatz übernehmen");
static Requirement A_22149   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern – Apotheke – FHIR-Validierung PKV-Abgabedatensatz");
static Requirement A_22150   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern - Apotheke – Signaturprüfung PKV-Abgabedatensatz");
static Requirement A_22151_01("E-Rezept-Fachdienst – Abrechnungsinformation ändern – Apotheke – Prüfung Signaturzertifikat PKV-Abgabedatensatz");
static Requirement A_22152   ("E-Rezept-Fachdienst - Abrechnungsinformation ändern – FHIR-Validierung ChargeItem");
static Requirement A_22615_02("E-Rezept-Fachdienst - Abrechnungsinformation ändern - Apotheke - Generierung AccessCode");
static Requirement A_22616_03("E-Rezept-Fachdienst - Abrechnungsinformation ändern - Apotheke - Prüfung AccessCode");
static Requirement A_23624   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern – Apotheke – kein AccessCode und Quittung");
// DELETE /ChargeItem
static Requirement A_22112   ("E-Rezept-Fachdienst - Abrechnungsinformation löschen - alles Löschen verbieten");
static Requirement A_22114   ("E-Rezept-Fachdienst - Abrechnungsinformation löschen – Prüfung KVNR");
static Requirement A_22115   ("E-Rezept-Fachdienst - Abrechnungsinformation löschen – Prüfung Telematik-ID");
static Requirement A_22117_01("E-Rezept-Fachdienst - Abrechnungsinformation löschen - zu löschende Ressourcen");
// GET /ChargeItem
static Requirement A_22119   ("E-Rezept-Fachdienst – Abrechnungsinformationen lesen – Versicherter – Filter KVNR");
static Requirement A_22121   ("E-Rezept-Fachdienst – Abrechnungsinformationen lesen - Suchkriterien");
static Requirement A_22122   ("E-Rezept-Fachdienst – Abrechnungsinformationen lesen – Response");
static Requirement A_22123   ("E-Rezept-Fachdienst – Abrechnungsinformationen lesen - Paging");
static Requirement A_22611_02("E-Rezept-Fachdienst - Abrechnungsinformation abrufen - Apotheke - Prüfung AccessCode");
// Requirements for endpoint /ChargeItem/<id>
static Requirement A_22125   ("E-Rezept-Fachdienst - Abrechnungsinformation lesen - Versicherter - Prüfung KVNR");
static Requirement A_22126   ("E-Rezept-Fachdienst - Abrechnungsinformation lesen - Apotheke - Prüfung Telematik-ID");
static Requirement A_22127   ("E-Rezept-Fachdienst - Abrechnungsinformation lesen - Versicherte - Signieren");
static Requirement A_22128_01("E-Rezept-Fachdienst - Abrechnungsinformation lesen - Apotheke - kein AccessCode und Quittung");
// PATCH /ChargeItem/<id>
static Requirement A_22875   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern (PATCH) – Rollenprüfung");
static Requirement A_22877   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern (PATCH) – Versicherter - Prüfung KVNR");
static Requirement A_22878   ("E-Rezept-Fachdienst - Abrechnungsinformation ändern (PATCH) – Zulässige Felder");
static Requirement A_22879   ("E-Rezept-Fachdienst – Abrechnungsinformation ändern (PATCH) - alles Ändern verbieten");

// Requirements for endpoint /Consent and /Consent/<id>
static Requirement A_22153   ("E-Rezept-Fachdienst - unzulässige Operationen Consent");
static Requirement A_22154   (" E-Rezept-Fachdienst - Consent loeschen - alles Loeschen verbieten");
static Requirement A_22156   ("E-Rezept-Fachdienst - Consent löschen – Prüfung KVNR");
static Requirement A_22157   ("E-Rezept-Fachdienst - Consent loeschen - Loeschen der bestehenden Abrechnungsinformationen");
static Requirement A_22158   ("E-Rezept-Fachdienst - Consent loeschen - Loeschen der Consent");
static Requirement A_22160   ("E-Rezept-Fachdienst - Consent lesen - Filter Consent auf KVNR des Versicherten");
static Requirement A_22162   ("E-Rezept-Fachdienst - Consent schreiben – nur eine Einwilligung CHARGCONS pro KVNR");
static Requirement A_22289   ("E-Rezept-Fachdienst - Consent schreiben - Prüfung KVNR");
static Requirement A_22350   ("E-Rezept-Fachdienst - Consent schreiben - Persistieren");
static Requirement A_22351   ("E-Rezept-Fachdienst - Consent schreiben - FHIR-Validierung");
static Requirement A_22874_01("E-Rezept-Fachdienst - Consent löschen - Prüfung category");

// Requirements for ACCESS_TOKEN checks
static Requirement A_20362    ("E-Rezept-Fachdienst - 'ACCESS_TOKEN' generelle Struktur");
static Requirement A_20365_01 ("E-Rezept-Fachdienst - Die Signatur des ACCESS_TOKEN ist zu prüfen");
static Requirement A_20369_01 ("E-Rezept-Fachdienst - Abbruch bei unerwarteten Inhalten");
static Requirement A_20370    ("E-Rezept-Fachdienst - Abbruch bei falschen Datentypen der Attribute");
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

// Workflow 169 / 209
static Requirement A_21360_01 ("E-Rezept-Fachdienst - Task abrufen - Flowtype 169 / 209 - keine Einlöseinformationen");
static Requirement A_21370 ("E-Rezept-Fachdienst - Prüfung Rezept-ID und Präfix gegen Flowtype");
static Requirement A_22102_01 ("E-Rezept-Fachdienst - E-Rezept löschen - Flowtype 169 / 209 - Versicherter - Statusprüfung");

// Requirements for endpoint /Subscription
static Requirement A_22362    ("E-Rezept-Fachdienst - Rollenpruefung Apotheke 'Create Subscription'");
static Requirement A_22363    ("E-Rezept-Fachdienst - Create Subscription 'validate telematic id'");
static Requirement A_22364    ("E-Rezept-Fachdienst - Create Subscription 'POST /Subscription response'");
static Requirement A_22365    ("E-Rezept-Fachdienst - Register Subscription 'pseudonym for telematic id'");
static Requirement A_22366    ("E-Rezept-Fachdienst - Create Subscription 'Bearer Token'");
static Requirement A_22367_02 ("E-Rezept-Fachdienst - Nachricht einstellen - Notification Apotheke");
static Requirement A_22383    ("E-Rezept-Fachdienst – Generierungsschlüssel – Pseudonym der Telematik-ID");

static Requirement A_22698    ("E-Rezept-Fachdienst - Erzeugung des Nutzerpseudonyms LEI");
static Requirement A_22975    ("Performance - Rohdaten-Performance-Bericht - Konfigurationpseudonymisierte Werte der Telematik-ID");
static Requirement A_23090_02 ("Performance - Rohdaten - Spezifika E-Rezept – Message (Rohdatenerfassung v.02)");

// Requirements for VSDM++
static Requirement A_23450    ("E-Rezept-Fachdienst - Rezepte lesen - Apotheke - VSDM - Prüfung Prüfungsnachweis");
static Requirement A_23451    ("E-Rezept-Fachdienst - Rezepte lesen - Apotheke - VSDM - Zeitraum Akzeptanz Prüfungsnachweis");
static Requirement A_23452_01 ("E-Rezept-Fachdienst - Rezepte lesen - Apotheke - VSDM - Filter KVNR");
static Requirement A_23454    ("E-Rezept-Fachdienst - Prüfung Prüfziffer");
static Requirement A_23455    ("E-Rezept-Fachdienst - Prüfung Prüfziffer - keine Prüfziffer");
static Requirement A_23456    ("E-Rezept-Fachdienst - Prüfung Prüfziffer - Berechnung HMAC der Prüfziffer");
static Requirement A_23492    ("E-Rezept-Fachdienst - VSDM HMAC-Schlüssel - Exportpaket einbringen");
static Requirement A_23493    ("E-Rezept-Fachdienst - VSDM HMAC-Schlüssel - Prüfung");
static Requirement A_23486    ("E-Rezept-Fachdienst - VSDM HMAC-Schlüssel - Ausgabe");

#endif

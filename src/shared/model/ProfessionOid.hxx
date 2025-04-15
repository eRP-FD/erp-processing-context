/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_PROFESSIONOID_HXX
#define ERP_PROCESSING_CONTEXT_PROFESSIONOID_HXX

#include <string_view>

namespace profession_oid
{
    // Tabelle 2: Tab_PKI_402 OID-Festlegung Rolle im X.509-Zertifikat für Berufsgruppen
    constexpr static std::string_view oid_arzt                       = "1.2.276.0.76.4.30";
    constexpr static std::string_view oid_zahnarzt                   = "1.2.276.0.76.4.31";
    constexpr static std::string_view oid_apotheker                  = "1.2.276.0.76.4.32";
    constexpr static std::string_view oid_apothekerassistent         = "1.2.276.0.76.4.33";
    constexpr static std::string_view oid_pharmazieingenieur         = "1.2.276.0.76.4.34";
    constexpr static std::string_view oid_pharm_techn_assistent      = "1.2.276.0.76.4.35";
    constexpr static std::string_view oid_pharm_kaufm_angestellter   = "1.2.276.0.76.4.36";
    constexpr static std::string_view oid_apothekenhelfer            = "1.2.276.0.76.4.37";
    constexpr static std::string_view oid_apothekenassistent         = "1.2.276.0.76.4.38";
    constexpr static std::string_view oid_pharm_assistent            = "1.2.276.0.76.4.39";
    constexpr static std::string_view oid_apothekenfacharbeiter      = "1.2.276.0.76.4.40";
    constexpr static std::string_view oid_pharmaziepraktikant        = "1.2.276.0.76.4.41";
    constexpr static std::string_view oid_famulant                   = "1.2.276.0.76.4.42";
    constexpr static std::string_view oid_pta_praktikant             = "1.2.276.0.76.4.43";
    constexpr static std::string_view oid_pka_auszubildender         = "1.2.276.0.76.4.44";
    constexpr static std::string_view oid_psychotherapeut            = "1.2.276.0.76.4.45";
    constexpr static std::string_view oid_ps_psychotherapeut         = "1.2.276.0.76.4.46";
    constexpr static std::string_view oid_kuj_psychotherapeut        = "1.2.276.0.76.4.47";
    constexpr static std::string_view oid_rettungsassistent          = "1.2.276.0.76.4.48";
    // GEMREQ-start oid_versicherter
    constexpr static std::string_view oid_versicherter               = "1.2.276.0.76.4.49";
    // GEMREQ-end oid_versicherter
    constexpr static std::string_view oid_notfallsanitaeter          = "1.2.276.0.76.4.178";
    constexpr static std::string_view oid_pfleger_hpc                = "1.2.276.0.76.4.232";
    constexpr static std::string_view oid_altenpfleger_hpc           = "1.2.276.0.76.4.233";
    constexpr static std::string_view oid_pflegefachkraft_hpc        = "1.2.276.0.76.4.234";
    constexpr static std::string_view oid_hebamme_hpc                = "1.2.276.0.76.4.235";
    constexpr static std::string_view oid_physiotherapeut_hpc        = "1.2.276.0.76.4.236";
    constexpr static std::string_view oid_augenoptiker_hpc           = "1.2.276.0.76.4.237";
    constexpr static std::string_view oid_hoerakustiker_hpc          = "1.2.276.0.76.4.238";
    constexpr static std::string_view oid_orthopaedieschuhmacher_hpc = "1.2.276.0.76.4.239";
    constexpr static std::string_view oid_orthopaedietechniker_hpc   = "1.2.276.0.76.4.240";
    constexpr static std::string_view oid_zahntechniker_hpc          = "1.2.276.0.76.4.241";
    constexpr static std::string_view oid_arztekammern               = "1.3.6.1.4.1.24796.4.11.1";

    // Tabelle 3: Tab_PKI_403-01 OID-Festlegung Institutionen im X.509-Zertifikat der SMC-B
    constexpr static std::string_view oid_praxis_arzt                        = "1.2.276.0.76.4.50";
    constexpr static std::string_view oid_zahnarztpraxis                     = "1.2.276.0.76.4.51";
    constexpr static std::string_view oid_praxis_psychotherapeut             = "1.2.276.0.76.4.52";
    constexpr static std::string_view oid_krankenhaus                        = "1.2.276.0.76.4.53";
    // GEMREQ-start oid_apotheke
    constexpr static std::string_view oid_oeffentliche_apotheke              = "1.2.276.0.76.4.54";
    constexpr static std::string_view oid_krankenhausapotheke                = "1.2.276.0.76.4.55";
    // GEMREQ-end oid_apotheke
    constexpr static std::string_view oid_bundeswehrapotheke                 = "1.2.276.0.76.4.56";
    constexpr static std::string_view oid_mobile_einrichtung_rettungsdienst  = "1.2.276.0.76.4.57";
    constexpr static std::string_view oid_bs_gematik                         = "1.2.276.0.76.4.58";
    constexpr static std::string_view oid_kostentraeger                      = "1.2.276.0.76.4.59";
    constexpr static std::string_view oid_leo_zahnaerzte                     = "1.2.276.0.76.4.187";
    constexpr static std::string_view oid_adv_ktr                            = "1.2.276.0.76.4.190";
    constexpr static std::string_view oid_leo_kassenaerztliche_vereinigung   = "1.2.276.0.76.4.210";
    constexpr static std::string_view oid_bs_gkv_spitzenverband              = "1.2.276.0.76.4.223";
    constexpr static std::string_view oid_leo_krankenhausverband             = "1.2.276.0.76.4.226";
    constexpr static std::string_view oid_leo_dktig                          = "1.2.276.0.76.4.227";
    constexpr static std::string_view oid_leo_dkg                            = "1.2.276.0.76.4.228";
    constexpr static std::string_view oid_leo_apothekerverband               = "1.2.276.0.76.4.224";
    constexpr static std::string_view oid_leo_dav                            = "1.2.276.0.76.4.225";
    constexpr static std::string_view oid_leo_baek                           = "1.2.276.0.76.4.229";
    constexpr static std::string_view oid_leo_aerztekammer                   = "1.2.276.0.76.4.230";
    constexpr static std::string_view oid_leo_zahnaerztekammer               = "1.2.276.0.76.4.231";
    constexpr static std::string_view oid_leo_kbv                            = "1.2.276.0.76.4.242";
    constexpr static std::string_view oid_leo_bzaek                          = "1.2.276.0.76.4.243";
    constexpr static std::string_view oid_leo_kzbv                           = "1.2.276.0.76.4.244";
    constexpr static std::string_view oid_institution_pflege                 = "1.2.276.0.76.4.245";
    constexpr static std::string_view oid_institution_geburtshilfe           = "1.2.276.0.76.4.246";
    constexpr static std::string_view oid_praxis_physiotherapeut             = "1.2.276.0.76.4.247";
    constexpr static std::string_view oid_institution_augenoptiker           = "1.2.276.0.76.4.248";
    constexpr static std::string_view oid_institution_hoerakustiker          = "1.2.276.0.76.4.249";
    constexpr static std::string_view oid_institution_orthopaedieschuhmacher = "1.2.276.0.76.4.250";
    constexpr static std::string_view oid_institution_orthopaedietechniker   = "1.2.276.0.76.4.251";
    constexpr static std::string_view oid_institution_zahntechniker          = "1.2.276.0.76.4.252";
    constexpr static std::string_view oid_institution_rettungsleitstellen    = "1.2.276.0.76.4.253";
    constexpr static std::string_view oid_sanitaetsdienst_bundeswehr         = "1.2.276.0.76.4.254";
    constexpr static std::string_view oid_institution_oegd                   = "1.2.276.0.76.4.255";
    constexpr static std::string_view oid_institution_arbeitsmedizin         = "1.2.276.0.76.4.256";
    constexpr static std::string_view oid_institution_vorsorge_reha          = "1.2.276.0.76.4.257";
    constexpr static std::string_view oid_epa_ktr                            = "1.2.276.0.76.4.XXX";

    constexpr static std::string_view inner_request_role_pharmacy = "pharmacy";
    constexpr static std::string_view inner_request_role_doctor = "doctor";
    constexpr static std::string_view inner_request_role_patient = "patient";
    constexpr static std::string_view inner_request_role_unknown = "";//NOLINT(readability-redundant-string-init)

    // Tabelle 6: Tab_PKI_406 OID-Festlegung technische Rolle in X.509-Zertifikaten
    constexpr static std::string_view oid_vsdd = "1.2.276.0.76.4.97";
    constexpr static std::string_view oid_ocsp = "1.2.276.0.76.4.99";
    constexpr static std::string_view oid_cms = "1.2.276.0.76.4.100";
    constexpr static std::string_view oid_ufs = "1.2.276.0.76.4.101";
    constexpr static std::string_view oid_ak = "1.2.276.0.76.4.103";
    constexpr static std::string_view oid_nk = "1.2.276.0.76.4.104";
    constexpr static std::string_view oid_kt = "1.2.276.0.76.4.105";
    constexpr static std::string_view oid_sak = "1.2.276.0.76.4.119";
    constexpr static std::string_view oid_int_vsdm = "1.2.276.0.76.4.159";
    constexpr static std::string_view oid_konfigdienst = "1.2.276.0.76.4.160";
    constexpr static std::string_view oid_vpnz_ti = "1.2.276.0.76.4.161";
    constexpr static std::string_view oid_vpnz_sis = "1.2.276.0.76.4.166";
    constexpr static std::string_view oid_cmfd = "1.2.276.0.76.4.174";
    constexpr static std::string_view oid_vzd_ti = "1.2.276.0.76.4.171";
    constexpr static std::string_view oid_komle = "1.2.276.0.76.4.172";
    constexpr static std::string_view oid_komle_recipient_emails = "1.2.276.0.76.4.173";
    constexpr static std::string_view oid_stamp = "1.2.276.0.76.4.184";
    constexpr static std::string_view oid_tsl_ti = "1.2.276.0.76.4.189";
    constexpr static std::string_view oid_wadg = "1.2.276.0.76.4.198";
    constexpr static std::string_view oid_epa_authn = "1.2.276.0.76.4.204";
    constexpr static std::string_view oid_epa_authz = "1.2.276.0.76.4.205";
    constexpr static std::string_view oid_epa_dvw = "1.2.276.0.76.4.206";
    constexpr static std::string_view oid_epa_mgmt = "1.2.276.0.76.4.207";
    constexpr static std::string_view oid_epa_recovery = "1.2.276.0.76.4.208";
    constexpr static std::string_view oid_epa_vau = "1.2.276.0.76.4.209";
    constexpr static std::string_view oid_vz_tsp = "1.2.276.0.76.4.215";
    constexpr static std::string_view oid_whk1_hsm = "1.2.276.0.76.4.216";
    constexpr static std::string_view oid_whk2_hsm = "1.2.276.0.76.4.217";
    constexpr static std::string_view oid_whk = "1.2.276.0.76.4.218";
    constexpr static std::string_view oid_sgd1_hsm = "1.2.276.0.76.4.219";
    constexpr static std::string_view oid_sgd2_hsm = "1.2.276.0.76.4.220";
    constexpr static std::string_view oid_sgd = "1.2.276.0.76.4.221";
    constexpr static std::string_view oid_erp_vau = "1.2.276.0.76.4.258";
    constexpr static std::string_view oid_erezept = "1.2.276.0.76.4.259";
    constexpr static std::string_view oid_idpd = "1.2.276.0.76.4.260";


/// @brief convert the profession OID into the Inner-Request-Role representaion (pharmacy, doctor, patient)
    constexpr std::string_view toInnerRequestRole(const std::string_view professionOid)
    {
        if (professionOid == oid_versicherter)
        {
            return inner_request_role_patient;
        }
        else if (professionOid == oid_arzt || professionOid == oid_zahnarzt ||
                 professionOid == oid_praxis_arzt || professionOid == oid_zahnarztpraxis ||
                 professionOid == oid_praxis_psychotherapeut || professionOid == oid_krankenhaus)
        {
            return inner_request_role_doctor;
        }
        else if (professionOid == oid_oeffentliche_apotheke || professionOid == oid_krankenhausapotheke ||
                 professionOid == oid_kostentraeger)
        {
            return inner_request_role_pharmacy;
        }
        else
        {
            return inner_request_role_unknown;
        }
    }
}


#endif //ERP_PROCESSING_CONTEXT_PROFESSIONOID_HXX

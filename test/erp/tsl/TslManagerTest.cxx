/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work

#include "erp/tsl/TslManager.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/tsl/OcspService.hxx"
#include "erp/tsl/TrustStore.hxx"
#include "erp/tsl/TslService.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Hash.hxx"
#include "test/erp/tsl/TslParsingExpectations.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockOcsp.hxx"
#include "test/mock/UrlRequestSenderMock.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"

#include <test_config.h>
#include <unordered_set>


namespace
{
    const std::unordered_set<std::string> expectedCertificates =
    {
        "/C=DE/O=ESS TEST-ONLY - NOT-VALID/CN=SGD1-ESS",
        "/C=DE/O=D-TRUST TEST-ONLY - NOT-VALID/CN=SGD2-RU",
        "/C=DE/O=D-TRUST TEST-ONLY - NOT-VALID/CN=SGD2-TU",
        "/C=DE/O=gematik GmbH TEST-ONLY - NOT-VALID/CN=SGD1-Aktor",
        "/C=DE/O=gematik GmbH TEST-ONLY - NOT-VALID/CN=SGD2-Aktor",
        "/C=DE/O=gematik NOT-VALID/CN=ehca OCSP Signer 3 TEST-ONLY",
        "/C=DE/O=D-Trust TEST-ONLY - NOT-VALID/CN=SGD2-Zentral Test",
        "/C=DE/O=arvato Systems GmbH NOT-VALID/CN=GEM.CRL8 TEST-ONLY",
        "/C=DE/O=arvato Systems GmbH NOT-VALID/CN=GEM.CRL9 TEST-ONLY",
        "/C=DE/O=medisign GmbH NOT-VALID/CN=MESIG.HBA.OCSP2 TEST-ONLY",
        "/C=DE/O=gematik NOT-VALID/CN=ehca OCSP Signer 5 ecc TEST-ONLY",
        "/C=DE/O=medisign GmbH NOT-VALID/CN=MESIG.SMCB.OCSP1 TEST-ONLY",
        "/C=DE/O=kubus IT NOT-VALID/CN=kubus IT OCSP Signer 4 TEST-ONLY",
        "/C=DE/O=kubus IT NOT-VALID/CN=kubus IT OCSP Signer 5 TEST-ONLY",
        "/C=DE/O=D-Trust TEST-ONLY - NOT-VALID/CN=SGD2-Zentral Referenz",
        "/C=DE/O=achelos GmbH NOT-VALID/CN=achelos.crl-signer01 TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/CN=egk.ocsp.telematik-test TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/CN=hba.ocsp.telematik-test TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/CN=vpn.ocsp.telematik-test TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/CN=komp.ocsp.telematik-test TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/CN=smcb.ocsp.telematik-test TEST-ONLY",
        "/C=DE/O=CGM TEST-ONLY - NOT-VALID/CN=sgd1-1.cgm.epa.telematik-test-hsm",
        "/C=DE/O=CGM TEST-ONLY - NOT-VALID/CN=sgd1-2.cgm.epa.telematik-test-hsm",
        "/C=DE/O=arvato Systems GmbH NOT-VALID/CN=TSL-CA OCSP-Signer4 TEST-ONLY",
        "/C=DE/O=arvato Systems GmbH NOT-VALID/CN=TSL-CA OCSP-Signer7 TEST-ONLY",
        "/C=DE/O=arvato Systems GmbH NOT-VALID/CN=TSL-CA OCSP-Signer8 TEST-ONLY",
        "/C=DE/O=IBM GmbH TU TEST-ONLY - NOT-VALID/CN=SGD1-IBM C.SGD-HSM.AUT TU",
        "/C=DE/O=D-Trust GmbH NOT-VALID/CN=D-Trust.HBA-CA3-OCSP-Signer TEST-ONLY",
        "/C=DE/O=D-Trust GmbH NOT-VALID/CN=D-Trust.SMCB-CA3-OCSP-Signer TEST-ONLY",
        "/C=DE/O=arvato Systems GmbH NOT-VALID/CN=Komp-PKI OCSP-Signer4 TEST-ONLY",
        "/C=DE/O=arvato Systems GmbH NOT-VALID/CN=Komp-PKI OCSP-Signer7 TEST-ONLY",
        R"(/CN=DEMDS CA f\xC3\xBCr \xC3\x84rzte 4:PN/OU=Testumgebung/O=medisign GmbH/C=DE)",
        "/C=DE/O=IBM GmbH TEST-ONLY - NOT-VALID/CN=SGD1-IBM C.SGD-HSM.AUT TEST_ONLY",
        "/C=DE/O=ITSG GmbH TEST-ONLY - NOT-VALID/CN=SGD1-itsg.epa.telematik-test-Sig",
        "/C=DE/O=BITMARCK Technik GmbH TEST-ONLY - NOT-VALID/CN=SGD1-EPA-RU-BITMARCK",
        "/C=DE/O=BITMARCK Technik GmbH TEST-ONLY - NOT-VALID/CN=SGD1-EPA-TU-BITMARCK",
        R"(/C=DE/O=medisign GmbH/OU=Testumgebung/CN=DEMDS CA f\xC3\xBCr Zahn\xC3\xA4rzte2:PN)",
        "/C=DE/O=Pseudo Federal Network Agency/CN=Pseudo German Trusted List Signer 4",
        "/C=DE/O=dgnservice GmbH/OU=Testumgebung/CN=dgnservice OCSP 7 Type A:PN",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP1 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP2 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP3 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP4 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP5 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP6 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP7 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP8 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP9 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.HBA-OCSP3 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.HBA-OCSP4 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH TEST-ONLY - NOT-VALID/CN=SGD1-AOK65-1",
        "/C=DE/O=CompuGroup Medical Deutschland AG TEST-ONLY - NOT-VALID/CN=SGD1-EPA-01",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/CN=BITMARCK.EGK-OCSP_SIGNER_5 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/CN=BITMARCK.EGK-OCSP_SIGNER_8 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP10 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP11 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP12 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP13 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP14 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP15 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP16 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP17 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP18 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.SMCB-OCSP3 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.SMCB-OCSP4 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/CN=BITMARCK.EGK-OCSP_SIGNER_11 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/CN=BITMARCK.EGK-OCSP_SIGNER_15 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/CN=BITMARCK.EGK-OCSP_SIGNER_16 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/CN=BITMARCK.EGK-OCSP_SIGNER_20 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/CN=BITMARCK.EGK-OCSP_SIGNER_21 TEST-ONLY",
        "/C=DE/O=ITSG GmbH TEST-ONLY - NOT-VALID/CN=SGD1-test.itsg.epa.telematik-test-Sig",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP201 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP202 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP203 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/CN=BITMARCK.EGK-OCSP_SIGNER_CA6 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/CN=BITMARCK.EGK-OCSP_SIGNER_CA9 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/CN=TSYSI.EGK-OCSP-Signer4 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/CN=TSYSI.EGK-OCSP-Signer5 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/CN=TSYSI.EGK-OCSP-Signer7 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/CN=TSYSI.HBA-OCSP-Signer1 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/CN=TSYSI.HBA-OCSP-Signer3 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/CN=TSYSI.SMCB-OCSP-Signer1 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/CN=TSYSI.SMCB-OCSP-Signer2 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/CN=TSYSI.SMCB-OCSP-Signer3 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-ALVI-OCSP1 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-ALVI-OCSP2 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-ALVI-OCSP3 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-ALVI-OCSP4 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/CN=TSYSI.HBA-OCSP-Signer1-1 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/CN=TSYSI.HBA-OCSP-Signer2-1 TEST-ONLY",
        R"(/CN=DEMDS CA f\xC3\xBCr Psychotherapeuten 2:PN/OU=Testumgebung/O=medisign GmbH/C=DE)",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/CN=TSYSI.EGK-CA5-OCSP-Signer TEST-ONLY",
        "/C=DE/O=BITMARCK TECHNIK GMBH TEST-ONLY - NOT-VALID/serialNumber=001/CN=SGD1-EPA-RU-BITMARCK",
        "/C=DE/O=BITMARCK TECHNIK GMBH TEST-ONLY - NOT-VALID/serialNumber=001/CN=SGD1-EPA-TU-BITMARCK",
        "/C=DE/O=BITMARCK TECHNIK GMBH TEST-ONLY - NOT-VALID/serialNumber=002/CN=SGD1-EPA-RU-BITMARCK",
        "/C=DE/O=BITMARCK TECHNIK GMBH TEST-ONLY - NOT-VALID/serialNumber=002/CN=SGD1-EPA-TU-BITMARCK",
        "/C=DE/O=Deutsche Telekom Security GmbH - G2 Los 3 NOT-VALID/CN=TSYSI.HBA-OCSP-Signer4 TEST-ONLY",
        "/C=DE/O=Deutsche Telekom Security GmbH - G2 Los 3 NOT-VALID/CN=TSYSI.SMCB-OCSP-Signer4 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=TSL-Signer-CA der Telematikinfrastruktur/CN=GEM.TSL-CA28 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Komponenten-CA der Telematikinfrastruktur/CN=GEM.KOMP-CA3 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Komponenten-CA der Telematikinfrastruktur/CN=GEM.KOMP-CA8 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Komponenten-CA der Telematikinfrastruktur/CN=GEM.KOMP-CA10 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Komponenten-CA der Telematikinfrastruktur/CN=GEM.KOMP-CA24 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Komponenten-CA der Telematikinfrastruktur/CN=GEM.KOMP-CA27 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Komponenten-CA der Telematikinfrastruktur/CN=GEM.KOMP-CA28 TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/OU=Komponenten-CA der Telematikinfrastruktur/CN=ACHELOS.KOMP-CA21 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=GEM.HBA-CA8 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=GEM.HBA-CA13 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=GEM.HBA-CA24 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=VPN-Zugangsdienst-CA der Telematikinfrastruktur/CN=GEM.VPNK-CA3 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=VPN-Zugangsdienst-CA der Telematikinfrastruktur/CN=GEM.VPNK-CA27 TEST-ONLY",
        "/C=DE/O=medisign GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=MESIG.HBA-CA1 TEST-ONLY",
        "/C=DE/O=D-Trust GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=D-Trust.HBA-CA2 TEST-ONLY",
        "/C=DE/O=D-Trust GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=D-Trust.HBA-CA3 TEST-ONLY",
        "/CN=ACLOS.HBA-CA015 TEST-ONLY/OU=Heilberufsausweis-CA der Telematikinfrastruktur/O=achelos GmbH NOT-VALID/C=DE",
        "/C=DE/O=achelos GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=ACHELOS.HBA-CA21 TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/OU=VPN-Zugangsdienst-CA der Telematikinfrastruktur/CN=ACHELOS.VPNK-CA21 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/OU=Trust Center Deutsche Telekom/CN=T-Systems eGK Test-CA 4 TEST-ONLY",
        "/C=DE/CN=GEM.EGK-CA08 TEST-ONLY/O=gematik GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=GEM.EGK-CA10 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=GEM.EGK-CA24 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=GEM.EGK-CA32 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=GEM.EGK-CA33 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=eGK alternative Vers-Ident-CA der Telematikinfrastruktur/CN=GEM.EGK-ALVI-CA10 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=eGK alternative Vers-Ident-CA der Telematikinfrastruktur/CN=GEM.EGK-ALVI-CA33 TEST-ONLY",
        "/C=DE/O=kubus IT GbR NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=KUBUS.EGK-CA2 TEST-ONLY",
        "/C=DE/O=kubus IT GbR NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=KUBUS.EGK-CA3 TEST-ONLY",
        "/C=DE/O=kubus IT GbR NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=KUBUS.EGK-CA4 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=GEM.SMCB-CA8 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=GEM.SMCB-CA9 TEST-ONLY",
        "/CN=ACLOS.EGK-CA015 TEST-ONLY/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/O=achelos GmbH NOT-VALID/C=DE",
        "/C=DE/O=achelos GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ACHELOS.EGK-CA21 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=GEM.SMCB-CA24 TEST-ONLY",
        "/C=DE/O=kubus IT GbR NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=kubus IT.EGK-CA1 TEST-ONLY",
        "/C=DE/O=medisign GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=MESIG.SMCB-CA1 TEST-ONLY",
        "/C=DE/O=D-Trust GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=D-Trust.SMCB-CA2 TEST-ONLY",
        "/C=DE/O=D-Trust GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=D-Trust.SMCB-CA3 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=ATOS.HBA-CA3 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=ATOS.HBA-CA4 TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=ACHELOS.SMCB-CA21 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=BITMARCK.EGK-CA5 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=BITMARCK.EGK-CA6 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=BITMARCK.EGK-CA8 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=BITMARCK.EGK-CA9 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=BITMARCK.EGK-CA11 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=BITMARCK.EGK-CA15 TEST-ONLY",
        "/C=DE/O=BITMARCK Technik GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=BITMARCK.EGK-CA16 TEST-ONLY",
        "/C=DE/O=BITMARCK TECHNIK GMBH NOT-VALID/OU=eGK alternative Vers-Ident-CA der Telematikinfrastruktur/CN=BITMARCK.EGK-ALVI-CA20 TEST-ONLY",
        "/C=DE/O=BITMARCK TECHNIK GMBH NOT-VALID/OU=eGK alternative Vers-Ident-CA der Telematikinfrastruktur/CN=BITMARCK.EGK-ALVI-CA21 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH - G2 Los 3 NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=TSYSI.HBA-CA1 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH - G2 Los 3 NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=TSYSI.HBA-CA2 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH - G2 Los 3 NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=TSYSI.HBA-CA3 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=TSYSI.EGK-CA2 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=TSYSI.EGK-CA3 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=TSYSI.EGK-CA4 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=TSYSI.EGK-CA5 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=TSYSI.EGK-CA7 TEST-ONLY",
        "/C=DE/O=Deutsche Telekom Security GmbH - G2 Los 3 NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=TSYSI.HBA-CA4 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA1 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA2 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA4 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA5 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA6 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA7 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA8 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA9 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=eGK alternative Vers-Ident-CA der Telematikinfrastruktur/CN=ATOS.EGK-ALVI-CA1 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=eGK alternative Vers-Ident-CA der Telematikinfrastruktur/CN=ATOS.EGK-ALVI-CA2 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=eGK alternative Vers-Ident-CA der Telematikinfrastruktur/CN=ATOS.EGK-ALVI-CA3 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=eGK alternative Vers-Ident-CA der Telematikinfrastruktur/CN=ATOS.EGK-ALVI-CA4 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA10 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA11 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA12 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA13 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA14 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA15 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA16 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA17 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA18 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA201 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA202 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA203 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=ATOS.SMCB-CA3 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=ATOS.SMCB-CA4 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH - G2 Los 3 NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=TSYSI.SMCB-CA1 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH - G2 Los 3 NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=TSYSI.SMCB-CA2 TEST-ONLY",
        "/C=DE/O=T-Systems International GmbH - G2 Los 3 NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=TSYSI.SMCB-CA3 TEST-ONLY",
        "/C=DE/O=Deutsche Telekom Security GmbH - G2 Los 3 NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=TSYSI.SMCB-CA4 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/serialNumber=00000-0001000000/CN=ATOS.EGK-CA3 TEST-ONLY",
        "/CN=ACLOS.SMCB-CA015 TEST-ONLY/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/O=achelos GmbH NOT-VALID/C=DE",
        "/C=DE/O=D-Trust GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=D-Trust.HBA-CA5 TEST-ONLY",
        "/C=DE/O=D-Trust GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=D-Trust.SMCB-CA5 TEST-ONLY",
        "/C=DE/O=ITSG GmbH TEST-ONLY - NOT-VALID/CN=SGD1-ITSG-RU2",
        "/C=DE/O=D-Trust GmbH NOT-VALID/CN=D-Trust.HBA-CA5-OCSP-Signer TEST-ONLY",
        "/C=DE/O=D-Trust GmbH NOT-VALID/CN=D-Trust.SMCB-CA5-OCSP-Signer TEST-ONLY",
        "/C=DE/O=IBM TEST-ONLY - NOT-VALID/serialNumber=10001-S/CN=SGD1-IBM C.SGD-HSM.AUT RU2 TEST_ONLY",
        "/C=DE/O=BITMARCK TECHNIK GMBH TEST-ONLY - NOT-VALID/CN=SGD1-EPA-QT-BITMARCK",
        "/C=DE/O=BITMARCK TECHNIK GMBH TEST-ONLY - NOT-VALID/CN=SGD1-EPA-RT-BITMARCK",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Komponenten-CA der Telematikinfrastruktur/CN=GEM.KOMP-CA50 TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/OU=eGK alternative Vers-Ident-CA der Telematikinfrastruktur/CN=ACHELOS.EGK-ALVI-CA20 TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/OU=eGK alternative Vers-Ident-CA der Telematikinfrastruktur/CN=ACHELOS.EGK-ALVI-CA21 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP19 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP20 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP204 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP205 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA19 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA20 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA204 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA205 TEST-ONLY",
        // The following certificate is explicitly inserted by test to allow to manipulate ocsp response validation
        R"(/C=DE/O=gematik Musterkasse1GKVNOT-VALID/OU=999567890/OU=X110506918/SN=D\xC3\xA5mmer-Meningham/GN=Sam/title=Dr./CN=Dr. Sam D\xC3\xA5mmer-MeninghamTEST-ONLY)",
        "/C=DE/CN=Pseudo German Trusted List Signer 5/O=Pseudo Federal Network Agency",
        "/C=DE/CN=Pseudo German Trusted List Signer 6/O=Pseudo Federal Network Agency",
        "/C=DE/O=medisign GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=MESIG.HBA-CA10 TEST-ONLY",
        "/C=DE/O=medisign GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=MESIG.SMCB-CA10 TEST-ONLY",
        "/C=DE/O=arvato Systems GmbH NOT-VALID/CN=GEM.CRL7 TEST-ONLY",
        "/C=DE/O=BITMARCK TECHNIK GMBH TEST-ONLY - NOT-VALID/CN=SGD1-EPA-TT-BITMARCK-01",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP21 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP22 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA21 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA22 TEST-ONLY",
        "/C=DE/O=medisign GmbH NOT-VALID/CN=MESIG.HBA-OCSP10 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=GEM.HBA-CA14 TEST-ONLY",
        "/C=DE/O=BITMARCK TECHNIK GMBH TEST-ONLY - NOT-VALID/CN=SGD1-EPA-QU-BITMARCK",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Komponenten-CA der Telematikinfrastruktur/CN=GEM.KOMP-CA41 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Komponenten-CA der Telematikinfrastruktur/CN=GEM.KOMP-CA51 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=GEM.HBA-CA41 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=GEM.HBA-CA51 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=VPN-Zugangsdienst-CA der Telematikinfrastruktur/CN=GEM.VPNK-CA28 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=VPN-Zugangsdienst-CA der Telematikinfrastruktur/CN=GEM.VPNK-CA41 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=VPN-Zugangsdienst-CA der Telematikinfrastruktur/CN=GEM.VPNK-CA50 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=VPN-Zugangsdienst-CA der Telematikinfrastruktur/CN=GEM.VPNK-CA51 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=GEM.EGK-CA41 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=GEM.EGK-CA51 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=eGK alternative Vers-Ident-CA der Telematikinfrastruktur/CN=GEM.EGK-ALVI-CA41 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=eGK alternative Vers-Ident-CA der Telematikinfrastruktur/CN=GEM.EGK-ALVI-CA51 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=GEM.SMCB-CA41 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=GEM.SMCB-CA51 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.HBA-OCSP5 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP23 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP24 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.SMCB-OCSP5 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/CN=ATOS.EGK-OCSP206 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=Komponenten-CA der Telematikinfrastruktur/CN=GEM.KOMP-CA54 TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/OU=Komponenten-CA der Telematikinfrastruktur/CN=ACHELOS.KOMP-CA20 TEST-ONLY",
        "/C=DE/O=gematik GmbH NOT-VALID/OU=VPN-Zugangsdienst-CA der Telematikinfrastruktur/CN=GEM.VPNK-CA54 TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=ACHELOS.HBA-CA20 TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/OU=VPN-Zugangsdienst-CA der Telematikinfrastruktur/CN=ACHELOS.VPNK-CA20 TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ACHELOS.EGK-CA20 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Heilberufsausweis-CA der Telematikinfrastruktur/CN=ATOS.HBA-CA5 TEST-ONLY",
        "/C=DE/O=achelos GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=ACHELOS.SMCB-CA20 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA23 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA24 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Elektronische Gesundheitskarte-CA der Telematikinfrastruktur/CN=ATOS.EGK-CA206 TEST-ONLY",
        "/C=DE/O=Atos Information Technology GmbH NOT-VALID/OU=Institution des Gesundheitswesens-CA der Telematikinfrastruktur/CN=ATOS.SMCB-CA5 TEST-ONLY"
    };
}


class TslManagerTest : public testing::Test
{
public:
    std::unique_ptr<EnvironmentVariableGuard> mCaDerPathGuard;

    void SetUp()
    {
        mCaDerPathGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificateIssuer.der");
    }

    void TearDown()
    {
        mCaDerPathGuard.reset();
    }

    void validateTslStore(X509Store& store)
    {
        ASSERT_NE(nullptr, store.getStore());

        STACK_OF(X509_OBJECT) *objects =
            X509_STORE_get0_objects(store.getStore());
        ASSERT_NE(nullptr, objects);

        int objectsCount = sk_X509_OBJECT_num(objects);

        std::unordered_set<std::string> foundCertificateSubjects;
        EXPECT_EQ(expectedCertificates.size(), objectsCount);
        for (int ind = 0; ind < objectsCount; ind++) {
            X509_OBJECT *object = sk_X509_OBJECT_value(objects, ind);
            ASSERT_NE(nullptr, object);

            if (X509_LU_X509 == X509_OBJECT_get_type(object))
            {
                X509* objectCertificate = X509_OBJECT_get0_X509(object);
                ASSERT_NE(nullptr, objectCertificate);
                X509_name_st* subjectName = X509_get_subject_name(objectCertificate);
                ASSERT_NE(nullptr, subjectName);

                char* subject = X509_NAME_oneline(subjectName, nullptr, 0);
                ASSERT_NE(nullptr, subject);
                std::string subjectString(subject);
                OPENSSL_free(subject);

                VLOG(1) << "trusted store certificate: " << subjectString;
                foundCertificateSubjects.emplace(subjectString);

                EXPECT_NE(expectedCertificates.end(), expectedCertificates.find(subjectString))
                                << "unexpected certificate in TSL: [" << subjectString << "]";
            }
        }

        for (const std::string& certificate : expectedCertificates)
        {
            EXPECT_NE(foundCertificateSubjects.end(), foundCertificateSubjects.find(certificate))
                            << "missing certificate in TSL: [" << certificate << "]";
        }
    }

    void validateBnaStore(X509Store& store)
    {
        ASSERT_NE(nullptr, store.getStore());

        STACK_OF(X509_OBJECT) *objects =
            X509_STORE_get0_objects(store.getStore());
        ASSERT_NE(nullptr, objects);

        int objectsCount = sk_X509_OBJECT_num(objects);
        size_t certificatesCount = 0;

        for (int ind = 0; ind < objectsCount; ind++) {
            X509_OBJECT *object = sk_X509_OBJECT_value(objects, ind);
            ASSERT_NE(nullptr, object);

            if (X509_LU_X509 == X509_OBJECT_get_type(object))
            {
                X509* objectCertificate = X509_OBJECT_get0_X509(object);
                ASSERT_NE(nullptr, objectCertificate);
                X509_name_st* subjectName = X509_get_subject_name(objectCertificate);
                ASSERT_NE(nullptr, subjectName);

                char* subject = X509_NAME_oneline(subjectName, nullptr, 0);
                ASSERT_NE(nullptr, subject);
                std::string subjectString(subject);
                OPENSSL_free(subject);

                VLOG(1) << "trusted store certificate: " << subjectString;
                certificatesCount++;
            }
        }

        // the long life test BNetzA-VL contains more certificates,
        // but there are withdrawn certificates
        // and the test version contains some duplicate certificates,
        // so there are only 185 unique not withdrawn certificates
        ASSERT_EQ(186, certificatesCount); // +1 certificate introduced explicitly by the test on runtime
    }
};


TEST_F(TslManagerTest, validTsl_Success)
{
    const std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    X509Store store = manager->getTslTrustedCertificateStore(TslMode::TSL, std::nullopt);
    validateTslStore(store);

    X509Store bnaStore = manager->getTslTrustedCertificateStore(TslMode::BNA, std::nullopt);
    validateBnaStore(bnaStore);
}


TEST_F(TslManagerTest, outdatedTslUpdate_Success)
{
    std::string newTslContent = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/TSL_valid.xml");

    bool hookIsCalled = false;
    const std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        nullptr,
        {[&hookIsCalled](void) mutable -> void {hookIsCalled = true;}},
        {},
        std::nullopt,
        TslTestHelper::createOutdatedTslTrustStore());

    // the new tsl must be imported
    X509Store store = manager->getTslTrustedCertificateStore(TslMode::TSL, std::nullopt);
    validateTslStore(store);

    X509Store bnaStore = manager->getTslTrustedCertificateStore(TslMode::BNA, std::nullopt);
    validateBnaStore(bnaStore);

    EXPECT_TRUE(hookIsCalled);
}


TEST_F(TslManagerTest, outdatedTslUpdateGetsOutdated_Fail)
{
    const std::string outdatedPath = std::string{TEST_DATA_DIR} + "/tsl/TSL_outdated.xml";
    const std::string newTslContent = FileHelper::readFileAsString(outdatedPath);

    const std::string tslDownloadUrl = "https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.xml";
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2", Hash::sha256(newTslContent)},
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", newTslContent},
            {tslDownloadUrl, newTslContent}});

    EXPECT_TSL_ERROR_THROW(TslTestHelper::createTslManager<TslManager>(
                               requestSender,
                               {},
                               {},
                               std::nullopt,
                               TslTestHelper::createOutdatedTslTrustStore(tslDownloadUrl)),
                           {TslErrorCode::VALIDITY_WARNING_2},
                           HttpStatus::InternalServerError);
}


TEST_F(TslManagerTest, validTslOutdatedBna_Fail)
{
    EnvironmentVariableGuard guard("ERP_TSL_INITIAL_CA_DER_PATH", std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificateIssuer.der");

    const std::string tslContent =
        FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/TSL_valid.xml");

    const std::string outdatedPath = std::string{TEST_DATA_DIR} + "/tsl/TSL_outdated.xml";
    const std::string bnaContent = FileHelper::readFileAsString(outdatedPath);
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", tslContent},
            {"https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2", Hash::sha256(tslContent)},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", Hash::sha256(bnaContent)}});

    const std::vector<TslErrorCode> errorCodes{TslErrorCode::VL_UPDATE_ERROR, TslErrorCode::TSL_SCHEMA_NOT_VALID};
    EXPECT_TSL_ERROR_THROW(TslTestHelper::createTslManager<TslManager>(requestSender),
                           errorCodes,
                           HttpStatus::InternalServerError);
}


TEST_F(TslManagerTest, validTslNoHttpSha2_Fail)
{
    const std::string tslContent =
        FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/TSL_valid.xml");
    const std::string bnaContent = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/BNA_valid.xml");
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", tslContent},
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.sha2", Hash::sha256(tslContent)},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", Hash::sha256(bnaContent)}});

    // fail if no sha2 per https is available
    const std::vector<TslErrorCode> errorCodes{TslErrorCode::TSL_INIT_ERROR, TslErrorCode::TSL_DOWNLOAD_ERROR};
    EXPECT_TSL_ERROR_THROW(TslTestHelper::createTslManager<TslManager>(requestSender),
                           errorCodes,
                           HttpStatus::InternalServerError);
}


TEST_F(TslManagerTest, loadTslWithInvalidTslSignerCertificate_Fail)
{
    const std::string tslContent =
        FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/TSL_wrongSigner.xml");
    const std::string bnaContent = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/BNA_valid.xml");
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", tslContent},
            {"https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2", Hash::sha256(tslContent)},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", Hash::sha256(bnaContent)}});

    const std::vector<TslErrorCode> errorCodes{TslErrorCode::TSL_INIT_ERROR, TslErrorCode::WRONG_EXTENDEDKEYUSAGE};
    EXPECT_TSL_ERROR_THROW(TslTestHelper::createTslManager<TslManager>(requestSender),
                           errorCodes,
                           HttpStatus::InternalServerError);
}


TEST_F(TslManagerTest, validateQesCertificate_Success)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());

    const Certificate certificateCA = Certificate::fromDerBase64String(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.sig-test.telematik-test:8080/status/qocsp",
             {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    ASSERT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}));
}


TEST_F(TslManagerTest, validateQesCertificate_UnexpectedCA_Fail)
{
    const std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/QES-CertificateWithUnexpectedCA.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}),
                           {TslErrorCode::CA_CERT_MISSING},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificate_UnexpectedCriticalExtension_Fail)
{
    const std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/QES-CertificateWithUnexpectedCritical.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}),
                           {TslErrorCode::CERT_TYPE_MISMATCH},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, oudatedInitialTslNoUpdate_Fail)
{
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{});

    const std::vector<TslErrorCode> errorCodes{TslErrorCode::VALIDITY_WARNING_2, TslErrorCode::TSL_DOWNLOAD_ERROR};
    EXPECT_TSL_ERROR_THROW(TslTestHelper::createTslManager<TslManager>(
                                requestSender,
                                {},
                                {},
                                std::nullopt,
                                TslTestHelper::createOutdatedTslTrustStore()),
                           errorCodes,
                           HttpStatus::InternalServerError);
}


TEST_F(TslManagerTest, validateQesCertificateOcspRevoked_Fail)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());

    const Certificate certificateCA = Certificate::fromDerBase64String(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.sig-test.telematik-test:8080/status/qocsp",
             {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::REVOKED}}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}),
                           {TslErrorCode::CERT_REVOKED},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspCertHashMissing_Fail)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());

    const Certificate certificateCA = Certificate::fromDerBase64String(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.sig-test.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::CERTHASH_MISSING}}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}),
                           {TslErrorCode::CERTHASH_EXTENSION_MISSING},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspCertHashMismatch_Fail)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());

    const Certificate certificateCA = Certificate::fromDerBase64String(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.sig-test.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::CERTHASH_MISMATCH}}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}),
                           {TslErrorCode::CERTHASH_MISMATCH},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspWrongCertId_Fail)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());

    const Certificate certificateCA = Certificate::fromDerBase64String(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.sig-test.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::WRONG_CERTID}}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}),
                           {TslErrorCode::OCSP_CHECK_REVOCATION_ERROR},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspWrongProducedAt_Fail)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());

    const Certificate certificateCA = Certificate::fromDerBase64String(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.sig-test.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::WRONG_PRODUCED_AT}}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}),
                           {TslErrorCode::PROVIDED_OCSP_RESPONSE_NOT_VALID},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspWrongThisUpdate_Fail)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());

    const Certificate certificateCA = Certificate::fromDerBase64String(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.sig-test.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::WRONG_THIS_UPDATE}}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}),
                           {TslErrorCode::PROVIDED_OCSP_RESPONSE_NOT_VALID},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspUnknown_Fail)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.sig-test.telematik-test:8080/status/qocsp",
             {}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}),
                           {TslErrorCode::CERT_UNKNOWN},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspNotAvailable_Fail)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());

    const Certificate certificateCA = Certificate::fromDerBase64String(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}),
                           {TslErrorCode::OCSP_NOT_AVAILABLE},
                           HttpStatus::InternalServerError);
}


TEST_F(TslManagerTest, validateQesCertificateOcspCertificateUnknown_Fail)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());

    const Certificate certificateCA = Certificate::fromDerBase64String(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.sig-test.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});
    TslTestHelper::removeCertificateFromTrustStore(TslTestHelper::getDefaultOcspCertificate(), *manager, TslMode::BNA);
    TslTestHelper::removeCertificateFromTrustStore(TslTestHelper::getDefaultOcspCertificateCa(), *manager, TslMode::BNA);

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}),
                           {TslErrorCode::OCSP_CERT_MISSING},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspCertificateSignedByBnaCa_Success)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());

    const Certificate certificateCA = Certificate::fromDerBase64String(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    const Certificate wansimOcspSigner =
        Certificate::fromPemString(FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/OcspWansimCertificate.pem"));
    const Certificate wansimOcspSignerCA =
        Certificate::fromDerBase64String(FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/OcspWansimCertificateCA.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.sig-test.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}},
        wansimOcspSigner);
    TslTestHelper::addCaCertificateToTrustStore(wansimOcspSignerCA, *manager, TslMode::BNA);

    EXPECT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}));
}


TEST_F(TslManagerTest, validateQesCertificateNoHistoryIgnoreTimestamp_Success)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());

    const Certificate certificateCA = Certificate::fromDerBase64String(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.sig-test.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});
    // set CA StatusStartingTime to the "valid from" timestamp of the certificate
    TslTestHelper::setCaCertificateTimestamp(
        model::Timestamp::fromFhirDateTime("2020-06-10T00:00:00Z").toChronoTimePoint(),
        certificateCA,
        *manager,
        TslMode::BNA);

    ASSERT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}));
}


TEST_F(TslManagerTest, validateQesCertificateOcspCached_Success)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());
    const Certificate certificateCA = Certificate::fromDerBase64String(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    const std::string tslContent =
        FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/TSL_valid.xml");
    const std::string bnaContent = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/BNA_valid.xml");
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", tslContent},
            {"https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2", Hash::sha256(tslContent)},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", Hash::sha256(bnaContent)}});

    const std::string ocspUrl = "http://ehca-testref.sig-test.telematik-test:8080/status/qocsp";
    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        requestSender,
        {},
        {
            {ocspUrl,
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    ASSERT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}));

    // turn OCSP-Responder off, the cached answer must be used
    requestSender->setUrlHandler(ocspUrl,
                                 [](const std::string&) -> ClientResponse
                                 {
                                     Header header;
                                     header.setStatus(HttpStatus::NotFound);
                                     header.setContentLength(0);
                                     return ClientResponse(header, "");
                                 });

    ASSERT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}));
}


TEST_F(TslManagerTest, validateQesCertificateNoTslUpdate_Success)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPemString(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toDerBase64String());
    const Certificate certificateCA = Certificate::fromDerBase64String(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    const std::string tslContent =
        FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/TSL_valid.xml");
    const std::string bnaContent = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/BNA_valid.xml");
    const std::string tslSha2Url = "https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2";
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", tslContent},
            {tslSha2Url, Hash::sha256(tslContent)},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", Hash::sha256(bnaContent)}});

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        requestSender,
        {},
        {
            {"http://ehca-testref.sig-test.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    ASSERT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}));

    // no update should happen at this point, throw exception if it happens
    requestSender->setUrlHandler(tslSha2Url,
                                 [](const std::string&) -> ClientResponse
                                 {
                                     // logic_error is not expected and is transported
                                     throw std::logic_error("error");
                                 });

    ASSERT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}));
}


TEST_F(TslManagerTest, validateIdpCertificateWrongType_Fail)
{
    X509Certificate x509Certificate =
        X509Certificate::createFromBase64(
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-WrongType.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::TSL, x509Certificate, {CertificateType::C_FD_SIG}),
                           {TslErrorCode::CERT_TYPE_MISMATCH},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateIdpCertificateExpired_Fail)
{
    Certificate idpCertificate =
        Certificate::fromPemString(
                FileHelper::readFileAsString(
                    std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Expired.pem"));

    Certificate idpCertificateCa = Certificate::fromDerBase64String(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim-CA.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ocsp-testref.tsl.telematik-test/ocsp",
                {{TslTestHelper::getTslSignerCertificate(), TslTestHelper::getTslSignerCACertificate(), MockOcsp::CertificateOcspTestMode::SUCCESS},
                    {idpCertificate, idpCertificateCa, MockOcsp::CertificateOcspTestMode::SUCCESS}}}}
    );

    TslTestHelper::addCaCertificateToTrustStore(
        idpCertificateCa,
        *manager,
        TslMode::TSL);

    X509Certificate x509Idp = X509Certificate::createFromBase64(idpCertificate.toDerBase64String());
    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(
                               TslMode::TSL,
                               x509Idp,
                               {CertificateType::C_FD_SIG}),
                           {TslErrorCode::CERTIFICATE_NOT_VALID_TIME},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateIdpCertificateMissingPolicy_Fail)
{
    Certificate idpCertificate =
        Certificate::fromPemString(
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-MissingPolicy.pem"));

    Certificate idpCertificateCa = Certificate::fromDerBase64String(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim-CA.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ocsp-testref.tsl.telematik-test/ocsp",
                {{TslTestHelper::getTslSignerCertificate(), TslTestHelper::getTslSignerCACertificate(), MockOcsp::CertificateOcspTestMode::SUCCESS},
                    {idpCertificate, idpCertificateCa, MockOcsp::CertificateOcspTestMode::SUCCESS}}}}
    );

    TslTestHelper::addCaCertificateToTrustStore(
        idpCertificateCa,
        *manager,
        TslMode::TSL);

    X509Certificate x509Idp = X509Certificate::createFromBase64(idpCertificate.toDerBase64String());
    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(
                                TslMode::TSL,
                                x509Idp,
                                {CertificateType::C_FD_SIG}),
                           {TslErrorCode::CERT_TYPE_INFO_MISSING},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateIdpCertificateUnknownCaKnownDn_Fail)
{
    Certificate idpCertificate =
        Certificate::fromDerBase64String(
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim.base64.der"));

    Certificate idpCertificateCa = Certificate::fromDerBase64String(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim-CA.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ocsp-testref.tsl.telematik-test/ocsp",
                {{TslTestHelper::getTslSignerCertificate(), TslTestHelper::getTslSignerCACertificate(), MockOcsp::CertificateOcspTestMode::SUCCESS},
                    {idpCertificate, idpCertificateCa, MockOcsp::CertificateOcspTestMode::SUCCESS}}}}
    );

    // the CA is inserted into trust store with different subject key identifier
    TslTestHelper::addCaCertificateToTrustStore(
        idpCertificateCa,
        *manager,
        TslMode::TSL,
        "differentSubjectKeyIdentifier");

    X509Certificate x509Idp = X509Certificate::createFromBase64(idpCertificate.toDerBase64String());
    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(
                                TslMode::TSL,
                                x509Idp,
                                {CertificateType::C_FD_SIG}),
                           {TslErrorCode::AUTHORITYKEYID_DIFFERENT},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateIdpCertificateCAUnsupportedType_Fail)
{
    Certificate idpCertificate =
        Certificate::fromDerBase64String(
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim.base64.der"));

    Certificate idpCertificateCa = Certificate::fromDerBase64String(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim-CA.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ocsp-testref.tsl.telematik-test/ocsp",
                {{TslTestHelper::getTslSignerCertificate(), TslTestHelper::getTslSignerCACertificate(), MockOcsp::CertificateOcspTestMode::SUCCESS},
                    {idpCertificate, idpCertificateCa, MockOcsp::CertificateOcspTestMode::SUCCESS}}}}
    );

    // the CA is inserted into trust store with different supported certificate type id
    TslTestHelper::addCaCertificateToTrustStore(
        idpCertificateCa,
        *manager,
        TslMode::TSL,
        std::nullopt,
        TslService::oid_egk_aut);

    X509Certificate x509Idp = X509Certificate::createFromBase64(idpCertificate.toDerBase64String());
    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(
                                TslMode::TSL,
                                x509Idp,
                                {CertificateType::C_FD_SIG}),
                           {TslErrorCode::CERT_TYPE_CA_NOT_AUTHORIZED},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateIdpCertificateCaNotAuthorized_Fail)
{
    Certificate idpCertificate =
        Certificate::fromDerBase64String(
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim.base64.der"));

    Certificate idpCertificateCa = Certificate::fromDerBase64String(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim-CA.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ocsp-testref.tsl.telematik-test/ocsp",
                {{TslTestHelper::getTslSignerCertificate(), TslTestHelper::getTslSignerCACertificate(), MockOcsp::CertificateOcspTestMode::SUCCESS},
                    {idpCertificate, idpCertificateCa, MockOcsp::CertificateOcspTestMode::SUCCESS}}}}
    );

    // the CA is inserted into trust store with different subject key identifier
    TslTestHelper::addCaCertificateToTrustStore(
        idpCertificateCa,
        *manager,
        TslMode::TSL,
        "differentSubjectKeyIdentifier");

    X509Certificate x509Idp = X509Certificate::createFromBase64(idpCertificate.toDerBase64String());
    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(
                                TslMode::TSL,
                                x509Idp,
                                {CertificateType::C_FD_SIG}),
                           {TslErrorCode::AUTHORITYKEYID_DIFFERENT},
                           HttpStatus::BadRequest);
}

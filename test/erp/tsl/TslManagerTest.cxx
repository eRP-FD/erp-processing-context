/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work

#include "shared/tsl/TslManager.hxx"
#include "shared/crypto/Certificate.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/tsl/OcspHelper.hxx"
#include "shared/tsl/TrustStore.hxx"
#include "shared/tsl/TslService.hxx"
#include "shared/tsl/error/TslError.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/Hash.hxx"
#include "mock/tsl/MockOcsp.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "test/erp/pc/CFdSigErpTestHelper.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceManager.hxx"
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
        "/C=DE/O=achelos GmbH NOT-VALID/OU=Fachanwendungsspezifischer Dienst-CA der Telematikinfrastruktur/CN=ACLOS.FD-CA1 TEST-ONLY",
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
        "/C=DE/O=achelos GmbH NOT-VALID/OU=Fachanwendungsspezifischer Dienst-CA der Telematikinfrastruktur/CN=ACLOS.FD-CA4 TEST-ONLY",
        // The following certificate is inserted in TSL and is used as TSL-Signer-CA of the Test-TSLs
        "/C=DE/ST=Berlin/L=Berlin/O=Example Inc./OU=IT/CN=Example Inc. Sub CA EC 1",
        "/C=DE/ST=Berlin/L=Berlin/O=Example Inc./OU=IT/CN=BNA signer",
        "/C=DE/O=Pseudo Federal Network Agency/CN=Pseudo German Trusted List Signer 4",
        "/CN=Pseudo German Trusted List Signer 11/O=Pseudo Federal Network Agency/C=DE",
        "/CN=Pseudo German Trusted List Signer 10/O=Pseudo Federal Network Agency/C=DE",
        "/CN=Pseudo German Trusted List Signer 9/O=Pseudo Federal Network Agency/C=DE",
    };
}


class TslManagerTest : public testing::Test
{
public:
    std::unique_ptr<EnvironmentVariableGuard> mCaDerPathGuard;

    void SetUp() override
    {
        mCaDerPathGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));
        testing::internal::CaptureStderr();
    }

    void TearDown() override
    {
        std::string output = testing::internal::GetCapturedStderr();
        EXPECT_FALSE(output.empty());
        EXPECT_EQ(output.find("wrong hash"), std::string::npos);
        EXPECT_EQ(output.find("not a valid hex string"), std::string::npos);
        mCaDerPathGuard.reset();
    }

    void validateTslStore(X509Store& store) //NOLINT(readability-function-cognitive-complexity)
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

                TVLOG(1) << "trusted store certificate: " << subjectString;
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

    void validateBnaStore(X509Store& store) //NOLINT(readability-function-cognitive-complexity)
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

                TVLOG(1) << "trusted store certificate: " << subjectString;
                certificatesCount++;
            }
        }

        // the long life test BNetzA-VL contains more certificates,
        // but there are withdrawn certificates
        // and the test version contains some duplicate certificates,
        // so there are only 201 unique not withdrawn certificates
        ASSERT_EQ(202, certificatesCount); // +1 certificate introduced explicitly by the test on runtime
    }

    static std::string sha256HexRN(const std::string& input)
    {
        return String::toHexString(Hash::sha256(input)) + "\r\n";
    }
};


TEST_F(TslManagerTest, validTsl_Success)
{
    const std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    X509Store store = manager->getTslTrustedCertificateStore(TslMode::TSL, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(validateTslStore(store));

    X509Store bnaStore = manager->getTslTrustedCertificateStore(TslMode::BNA, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(validateBnaStore(bnaStore));
}


TEST_F(TslManagerTest, outdatedTslUpdate_Success)
{
    bool hookIsCalled = false;
    const std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        nullptr,
        {[&hookIsCalled](void) mutable -> void {hookIsCalled = true;}},
        {},
        std::nullopt,
        TslTestHelper::createOutdatedTslTrustStore());

    // the new tsl must be imported
    X509Store store = manager->getTslTrustedCertificateStore(TslMode::TSL, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(validateTslStore(store));

    X509Store bnaStore = manager->getTslTrustedCertificateStore(TslMode::BNA, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(validateBnaStore(bnaStore));

    EXPECT_TRUE(hookIsCalled);
}


TEST_F(TslManagerTest, outdatedTslUpdateGetsOutdated_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string newTslContent =
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_outdated.xml");

    const std::string tslDownloadUrl = "https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.xml";
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2", sha256HexRN(newTslContent)},
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", newTslContent},
            {tslDownloadUrl, newTslContent}});

    auto mgr = TslTestHelper::createTslManager<TslManager>(
                               requestSender,
                               {},
                               {},
                               std::nullopt,
                               TslTestHelper::createOutdatedTslTrustStore(tslDownloadUrl));

    EXPECT_TSL_ERROR_THROW(mgr->updateTrustStoresOnDemand(),
                           {TslErrorCode::VALIDITY_WARNING_2},
                           HttpStatus::InternalServerError);
}


TEST_F(TslManagerTest, validTslOutdatedBna_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    EnvironmentVariableGuard guard("ERP_TSL_INITIAL_CA_DER_PATH", ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));

    const std::string tslContent =
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_valid.xml");

    const std::string bnaContent =
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_outdated.xml");
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", tslContent},
            {"https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2", sha256HexRN(tslContent)},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", sha256HexRN(bnaContent)}});

    const std::vector<TslErrorCode> errorCodes{TslErrorCode::VL_UPDATE_ERROR, TslErrorCode::TSL_SCHEMA_NOT_VALID};
    auto mgr = TslTestHelper::createTslManager<TslManager>(requestSender);

    EXPECT_TSL_ERROR_THROW(mgr->updateTrustStoresOnDemand(),
                           errorCodes,
                           HttpStatus::InternalServerError);
}


TEST_F(TslManagerTest, validTslNoHttpSha2_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string newTslContent =
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_outdated.xml");

    const std::string tslDownloadUrl = "https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.xml";
    std::shared_ptr<UrlRequestSenderMock> requestSender =
        std::make_shared<UrlRequestSenderMock>(std::unordered_map<std::string, std::string>{
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", newTslContent},
            {tslDownloadUrl, newTslContent}});
    // only an outdated trust store will try to download the sha2 file
    auto mgr = TslTestHelper::createTslManager<TslManager>(requestSender, {}, {}, std::nullopt,
                                                           TslTestHelper::createOutdatedTslTrustStore(tslDownloadUrl));

    // fail if no sha2 per https is available
    const std::vector<TslErrorCode> errorCodes{TslErrorCode::VALIDITY_WARNING_2, TslErrorCode::TSL_DOWNLOAD_ERROR};
    EXPECT_TSL_ERROR_THROW(mgr->updateTrustStoresOnDemand(), errorCodes, HttpStatus::InternalServerError);
}


TEST_F(TslManagerTest, loadTslWithInvalidTslSignerCertificate_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string tslContent =
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_wrongSigner.xml");
    const std::string bnaContent = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/BNA_valid.xml");
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", tslContent},
            {"https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2", sha256HexRN(tslContent)},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", sha256HexRN(bnaContent)}});

    const std::vector<TslErrorCode> errorCodes{TslErrorCode::TSL_INIT_ERROR, TslErrorCode::WRONG_EXTENDEDKEYUSAGE};
    auto mgr = TslTestHelper::createTslManager<TslManager>(requestSender);

    EXPECT_TSL_ERROR_THROW(mgr->updateTrustStoresOnDemand(),
                           errorCodes,
                           HttpStatus::InternalServerError);

}


TEST_F(TslManagerTest, validateQesCertificate_Success)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp",
             {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    ASSERT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()));
}


TEST_F(TslManagerTest, validateQesCertificate_OutdatedCAButValid_Success)
{
    const Certificate certificate =
        Certificate::fromBinaryDer(
            ResourceManager::instance().getStringResource(
                "test/generated_pki/outdated_ca_ec/certificates/qes_cert1_ec/qes_cert1_ec.der"));
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA =
        Certificate::fromBinaryDer(
            ResourceManager::instance().getStringResource("test/generated_pki/outdated_ca_ec/ca.der"));

    const std::string tslContent =
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_valid.xml");
    const std::string bnaContent = ResourceManager::instance().getStringResource("test/generated_pki/tsl/BNA_EC_valid.xml");
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", tslContent},
            {"https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2", sha256HexRN(tslContent)},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", sha256HexRN(bnaContent)}});

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        requestSender,
        {},
        {
            {"http://ocsp.test.ibm.de/",
             {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    ASSERT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()));
}


TEST_F(TslManagerTest, validateQesCertificate_OutdatedCAInvalid_Failure)
{
    const Certificate certificate =
        Certificate::fromBinaryDer(
            ResourceManager::instance().getStringResource(
                "test/generated_pki/outdated_ca_ec/certificates/qes_cert2_ec/qes_cert2_ec.der"));
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA =
        Certificate::fromBinaryDer(
            ResourceManager::instance().getStringResource("test/generated_pki/outdated_ca_ec/ca.der"));

    const std::string tslContent =
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_valid.xml");
    const std::string bnaContent = ResourceManager::instance().getStringResource("test/generated_pki/tsl/BNA_EC_valid.xml");
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", tslContent},
            {"https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2", sha256HexRN(tslContent)},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", sha256HexRN(bnaContent)}});

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        requestSender,
        {},
        {
            {"http://ocsp.test.ibm.de/",
             {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::CERTIFICATE_NOT_VALID_MATH},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificate_UnexpectedCA_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/QES-CertificateWithUnexpectedCA.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::CA_CERT_MISSING},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificate_UnexpectedCriticalExtension_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/QES-CertificateWithUnexpectedCritical.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::CERT_TYPE_MISMATCH},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, oudatedInitialTslNoUpdate_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{});

    const std::vector<TslErrorCode> errorCodes{TslErrorCode::VALIDITY_WARNING_2, TslErrorCode::TSL_DOWNLOAD_ERROR};
    auto mgr = TslTestHelper::createTslManager<TslManager>(
                                requestSender,
                                {},
                                {},
                                std::nullopt,
                                TslTestHelper::createOutdatedTslTrustStore());

    EXPECT_TSL_ERROR_THROW(mgr->updateTrustStoresOnDemand(),
                           errorCodes,
                           HttpStatus::InternalServerError);
}


TEST_F(TslManagerTest, validateQesCertificateOcspRevoked_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp",
             {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::REVOKED}}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::CERT_REVOKED},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspCertHashMissing_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::CERTHASH_MISSING}}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::CERTHASH_EXTENSION_MISSING},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspCertHashMismatch_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::CERTHASH_MISMATCH}}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::CERTHASH_MISMATCH},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspWrongCertId_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::WRONG_CERTID}}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::OCSP_CHECK_REVOCATION_ERROR},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspWrongProducedAt_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::WRONG_PRODUCED_AT}}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::PROVIDED_OCSP_RESPONSE_NOT_VALID},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspWrongThisUpdate_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::WRONG_THIS_UPDATE}}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::PROVIDED_OCSP_RESPONSE_NOT_VALID},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspUnknown_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp",
             {}}});

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::CERT_UNKNOWN},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspNotAvailable_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES},
                                                      TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::OCSP_NOT_AVAILABLE}, HttpStatus::BackendCallFailed);
}


TEST_F(TslManagerTest, validateQesCertificateOcspCertificateUnknown_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    std::weak_ptr<TslManager> mgrWeakPtr{manager};
    manager->addPostUpdateHook([mgrWeakPtr] {
        auto tslManager = mgrWeakPtr.lock();
        if (! tslManager)
            return;
        TslTestHelper::removeCertificateFromTrustStore(TslTestHelper::getDefaultOcspCertificate(), *tslManager,
                                                       TslMode::BNA);
        TslTestHelper::removeCertificateFromTrustStore(TslTestHelper::getDefaultOcspCertificateCa(), *tslManager,
                                                       TslMode::BNA);
    });

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::OCSP_CERT_MISSING},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateQesCertificateOcspCertificateSignedByBnaCa_Success)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    const Certificate wansimOcspSigner =
        Certificate::fromPem(FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/OcspWansimCertificate.pem"));
    const Certificate wansimOcspSignerCA =
        Certificate::fromBase64Der(FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/OcspWansimCertificateCA.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}},
        wansimOcspSigner);

    std::weak_ptr<TslManager> mgrWeakPtr{manager};
    manager->addPostUpdateHook([mgrWeakPtr, wansimOcspSignerCA] {
        auto tslManager = mgrWeakPtr.lock();
        if (! tslManager)
            return;
        TslTestHelper::addCaCertificateToTrustStore(wansimOcspSignerCA, *tslManager, TslMode::BNA);
    });

    EXPECT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()));
}


TEST_F(TslManagerTest, validateQesCertificateNoHistoryIgnoreTimestamp_Success)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});
    // set CA StatusStartingTime to the "valid from" timestamp of the certificate
    std::weak_ptr<TslManager> mgrWeakPtr{manager};

    manager->addPostUpdateHook([mgrWeakPtr, certificateCA] {
        auto tslManager = mgrWeakPtr.lock();
        if (! tslManager)
            return;
        TslTestHelper::setCaCertificateTimestamp(
            model::Timestamp::fromFhirDateTime("2020-06-10T00:00:00Z").toChronoTimePoint(), certificateCA, *tslManager,
            TslMode::BNA);
    });

    ASSERT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()));
}


TEST_F(TslManagerTest, validateQesCertificateOcspCached_Success)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());
    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    const std::string tslContent =
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_valid.xml");
    const std::string bnaContent = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/BNA_valid.xml");
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", tslContent},
            {"https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2", sha256HexRN(tslContent)},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", sha256HexRN(bnaContent)}});

    const std::string ocspUrl = "http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp";
    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        requestSender,
        {},
        {
            {ocspUrl,
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    ASSERT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()));

    // turn OCSP-Responder off, the cached answer must be used
    requestSender->setUrlHandler(ocspUrl,
                                 [](const std::string&) -> ClientResponse
                                 {
                                     Header header;
                                     header.setStatus(HttpStatus::NotFound);
                                     header.setContentLength(0);
                                     return ClientResponse(header, "");
                                 });

    ASSERT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()));
}


TEST_F(TslManagerTest, validateQesCertificateNoTslUpdate_Success)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());
    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    const std::string tslContent =
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_valid.xml");
    const std::string bnaContent = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/BNA_valid.xml");
    const std::string tslSha2Url = "https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2";
    std::shared_ptr<UrlRequestSenderMock> requestSender = std::make_shared<UrlRequestSenderMock>(
        std::unordered_map<std::string, std::string>{
            {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", tslContent},
            {tslSha2Url, sha256HexRN(tslContent)},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
            {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", sha256HexRN(bnaContent)}});

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        requestSender,
        {},
        {
            {"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp",
                {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    ASSERT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()));

    // no update should happen at this point, throw exception if it happens
    requestSender->setUrlHandler(tslSha2Url,
                                 [](const std::string&) -> ClientResponse
                                 {
                                     // logic_error is not expected and is transported
                                     throw std::logic_error("error");
                                 });

    ASSERT_NO_THROW(manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, TslTestHelper::getDefaultTestOcspCheckDescriptor()));
}


TEST_F(TslManagerTest, validateIdpCertificateWrongType_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    X509Certificate x509Certificate =
        X509Certificate::createFromBase64(
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-WrongType.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(TslMode::TSL, x509Certificate, {CertificateType::C_FD_SIG}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::CERT_TYPE_MISMATCH},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateIdpCertificateExpired_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    Certificate idpCertificate =
        Certificate::fromPem(
                FileHelper::readFileAsString(
                    std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Expired.pem"));

    Certificate idpCertificateCa = Certificate::fromPem(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim-CA.pem"));

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

    X509Certificate x509Idp = X509Certificate::createFromBase64(idpCertificate.toBase64Der());
    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(
                               TslMode::TSL,
                               x509Idp,
                               {CertificateType::C_FD_SIG},
                               TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::CERTIFICATE_NOT_VALID_TIME},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateIdpCertificateMissingPolicy_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    Certificate idpCertificate =
        Certificate::fromPem(
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-MissingPolicy.pem"));

    Certificate idpCertificateCa = Certificate::fromPem(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim-CA.pem"));

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

    X509Certificate x509Idp = X509Certificate::createFromBase64(idpCertificate.toBase64Der());
    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(
                                TslMode::TSL,
                                x509Idp,
                                {CertificateType::C_FD_SIG}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::CERT_TYPE_INFO_MISSING},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateIdpCertificateUnknownCaKnownDn_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    Certificate idpCertificate =
        Certificate::fromPem(
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim.pem"));

    Certificate idpCertificateCa = Certificate::fromPem(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim-CA.pem"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ocsp-testref.tsl.telematik-test/ocsp",
                {{TslTestHelper::getTslSignerCertificate(), TslTestHelper::getTslSignerCACertificate(), MockOcsp::CertificateOcspTestMode::SUCCESS},
                    {idpCertificate, idpCertificateCa, MockOcsp::CertificateOcspTestMode::SUCCESS}}}}
    );

    // the CA is inserted into trust store with different subject key identifier
    std::weak_ptr<TslManager> mgrWeakPtr{manager};

    manager->addPostUpdateHook([mgrWeakPtr, idpCertificateCa] {
        auto tslManager = mgrWeakPtr.lock();
        if (! tslManager)
            return;
        TslTestHelper::addCaCertificateToTrustStore(
            idpCertificateCa,
            *tslManager,
            TslMode::TSL,
            "differentSubjectKeyIdentifier");
    });

    X509Certificate x509Idp = X509Certificate::createFromBase64(idpCertificate.toBase64Der());
    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(
                                TslMode::TSL,
                                x509Idp,
                                {CertificateType::C_FD_SIG}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::AUTHORITYKEYID_DIFFERENT},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateIdpCertificateCAUnsupportedType_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    Certificate idpCertificate =
        Certificate::fromPem(
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim.pem"));

    Certificate idpCertificateCa = Certificate::fromPem(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim-CA.pem"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ocsp-testref.tsl.telematik-test/ocsp",
                {{TslTestHelper::getTslSignerCertificate(), TslTestHelper::getTslSignerCACertificate(), MockOcsp::CertificateOcspTestMode::SUCCESS},
                    {idpCertificate, idpCertificateCa, MockOcsp::CertificateOcspTestMode::SUCCESS}}}}
    );

    // the CA is inserted into trust store with different supported certificate type id
    std::weak_ptr<TslManager> mgrWeakPtr{manager};

    manager->addPostUpdateHook([mgrWeakPtr, idpCertificateCa] {
        auto tslManager = mgrWeakPtr.lock();
        if (! tslManager)
            return;
        TslTestHelper::addCaCertificateToTrustStore(
            idpCertificateCa,
            *tslManager,
            TslMode::TSL,
            std::nullopt,
            TslService::oid_egk_aut);
    });

    X509Certificate x509Idp = X509Certificate::createFromBase64(idpCertificate.toBase64Der());
    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(
                                TslMode::TSL,
                                x509Idp,
                                {CertificateType::C_FD_SIG}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::CERT_TYPE_CA_NOT_AUTHORIZED},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, validateIdpCertificateCaNotAuthorized_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    Certificate idpCertificate =
        Certificate::fromPem(
            FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim.pem"));

    Certificate idpCertificateCa = Certificate::fromPem(
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/IDP-Wansim-CA.pem"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ocsp-testref.tsl.telematik-test/ocsp",
                {{TslTestHelper::getTslSignerCertificate(), TslTestHelper::getTslSignerCACertificate(), MockOcsp::CertificateOcspTestMode::SUCCESS},
                    {idpCertificate, idpCertificateCa, MockOcsp::CertificateOcspTestMode::SUCCESS}}}}
    );

    // the CA is inserted into trust store with different subject key identifier
    std::weak_ptr<TslManager> mgrWeakPtr{manager};

    manager->addPostUpdateHook([mgrWeakPtr, idpCertificateCa] {
        auto tslManager = mgrWeakPtr.lock();
        if (! tslManager)
            return;
        TslTestHelper::addCaCertificateToTrustStore(idpCertificateCa, *tslManager, TslMode::TSL,
                                                    "differentSubjectKeyIdentifier");
    });

    X509Certificate x509Idp = X509Certificate::createFromBase64(idpCertificate.toBase64Der());
    EXPECT_TSL_ERROR_THROW(manager->verifyCertificate(
                                TslMode::TSL,
                                x509Idp,
                                {CertificateType::C_FD_SIG}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
                           {TslErrorCode::AUTHORITYKEYID_DIFFERENT},
                           HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, revokedOcspResponseStatus_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    auto cert = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErp());
    X509Certificate certX509 = X509Certificate::createFromBase64(cert.toBase64Der());
    auto certCA = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErpSigner());
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{CFdSigErpTestHelper::cFdSigErpPrivateKey()});
    const std::string ocspUrl(CFdSigErpTestHelper::cFsSigErpOcspUrl());

    // trigger revoked status in OCSP-Responder for the certificate
    std::shared_ptr<TslManager> tslManager = TslTestHelper::createTslManager<TslManager>(
        {}, {}, {{ocspUrl, {{cert, certCA, MockOcsp::CertificateOcspTestMode::REVOKED}}}});

    EXPECT_TSL_ERROR_THROW(
        tslManager->getCertificateOcspResponse(TslMode::TSL, certX509, {CertificateType::C_FD_OSIG}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
        {TslErrorCode::CERT_REVOKED},
        HttpStatus::BadRequest);

    // the second call is done to test handling of the OCSP-Response from cache
    EXPECT_TSL_ERROR_THROW(
        tslManager->getCertificateOcspResponse(TslMode::TSL, certX509, {CertificateType::C_FD_OSIG}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
        {TslErrorCode::CERT_REVOKED},
        HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, unknownOcspResponseStatus_Fail)//NOLINT(readability-function-cognitive-complexity)
{
    auto cert = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErp());
    X509Certificate certX509 = X509Certificate::createFromBase64(cert.toBase64Der());
    auto certCA = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErpSigner());
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{CFdSigErpTestHelper::cFdSigErpPrivateKey()});
    const std::string ocspUrl(CFdSigErpTestHelper::cFsSigErpOcspUrl());

    // trigger unknown status in OCSP-Responder for the certificate
    std::shared_ptr<TslManager> tslManager = TslTestHelper::createTslManager<TslManager>(
        {}, {}, {{ocspUrl, {}}});

    EXPECT_TSL_ERROR_THROW(
        tslManager->getCertificateOcspResponse(TslMode::TSL, certX509, {CertificateType::C_FD_OSIG}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
        {TslErrorCode::CERT_UNKNOWN},
        HttpStatus::BadRequest);

    // the second call is done to test handling of the OCSP-Response from cache
    EXPECT_TSL_ERROR_THROW(
        tslManager->getCertificateOcspResponse(TslMode::TSL, certX509, {CertificateType::C_FD_OSIG}, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
        {TslErrorCode::CERT_UNKNOWN},
        HttpStatus::BadRequest);
}


TEST_F(TslManagerTest, fromCacheFlag)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string certificatePem = FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    const std::string tslContent =
        ResourceManager::instance().getStringResource("test/generated_pki/tsl/TSL_valid.xml");
    const std::string bnaContent = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/BNA_valid.xml");

    auto requestSender = std::make_shared<UrlRequestSenderMock>(std::unordered_map<std::string, std::string>{
        {TslTestHelper::shaDownloadUrl, sha256HexRN(tslContent)},
        {TslTestHelper::tslDownloadUrl, tslContent},
        {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
        {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", sha256HexRN(bnaContent)},
    });
    TslTestHelper::setOcspUslRequestHandlerTslSigner(*requestSender);
    std::string ocspUrl = "http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp";

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        requestSender, {}, {{ocspUrl, {{certificate, certificateCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    {
        auto responseData = manager->getCertificateOcspResponse(
            TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES},
            {OcspCheckDescriptor::OcspCheckMode::FORCE_OCSP_REQUEST_ALLOW_CACHE,
            {std::nullopt, std::chrono::seconds{842000}},
            {}});

        EXPECT_FALSE(responseData.fromCache);
    }
    // force a new OCSP request and simulate a network error
    requestSender->setUrlHandler(ocspUrl, [](const std::string&) -> ClientResponse {
        Header header;
        header.setStatus(HttpStatus::NetworkConnectTimeoutError);
        header.setContentLength(0);
        return {header, ""};
    });
    {
        auto responseData = manager->getCertificateOcspResponse(
            TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES},
            {OcspCheckDescriptor::OcspCheckMode::FORCE_OCSP_REQUEST_ALLOW_CACHE,
            {std::nullopt, std::chrono::seconds{842000}},
            {}});

        EXPECT_TRUE(responseData.fromCache);
    }
}


TEST_F(TslManagerTest, permitOutdatedProducedAt)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string certificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem");
    const Certificate certificate = Certificate::fromPem(certificatePem);
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());

    const Certificate certificateCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    const auto certPairValid = MockOcsp::CertificatePair{
        .certificate = certificate,
        .issuer = certificateCA,
        .testMode = MockOcsp::CertificateOcspTestMode::SUCCESS
    };

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {}, {}, {{"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp", {certPairValid}}});

    OcspCertidPtr certId(OCSP_cert_to_id(nullptr, certificate.toX509(), certificateCA.toX509()));

    auto checkDescriptor = TslTestHelper::getDefaultTestOcspCheckDescriptor();
    checkDescriptor.mode = OcspCheckDescriptor::PROVIDED_OR_CACHE_REQUEST_IF_OUTDATED;

// GEMREQ-start A_24913
    auto ocspCert = TslTestHelper::getDefaultOcspCertificate();
    auto ocspKey = TslTestHelper::getDefaultOcspPrivateKey();
    using enum MockOcsp::CertificateOcspTestMode;
    // in this PROVIDED_OR_CACHE_REQUEST_IF_OUTDATED mode, an outdated ocsp response is allowed
    // and falling back to the cache/ocsp request
    {
        auto certPairOutdated = certPairValid;
        certPairOutdated.testMode = WRONG_PRODUCED_AT;
        auto response = MockOcsp::create(certId.get(), {certPairOutdated}, ocspCert, ocspKey).toDer();
        checkDescriptor.providedOcspResponse = OcspHelper::stringToOcspResponse(response);
        ASSERT_NE(checkDescriptor.providedOcspResponse, nullptr);

        ASSERT_NO_THROW(
            manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, checkDescriptor));
    }

    // fail on any other error
    for (auto testMode : {REVOKED, CERTHASH_MISMATCH, CERTHASH_MISSING, WRONG_CERTID, WRONG_THIS_UPDATE})
    {
        auto certPairInvalid = certPairValid;
        certPairInvalid.testMode = testMode;
        auto response = MockOcsp::create(certId.get(), {certPairInvalid}, ocspCert, ocspKey).toDer();
        checkDescriptor.providedOcspResponse = OcspHelper::stringToOcspResponse(response);
        EXPECT_THROW(
            manager->verifyCertificate(TslMode::BNA, x509Certificate, {CertificateType::C_HP_QES}, checkDescriptor),
            TslError);
    }
// GEMREQ-end A_24913
}


/**
 * Test the download of the TSL followed by the BNA download.
 * To test, adjust template_TSL_valid.xml to point to the download server in ServiceSupplyPoint and
 * make both XML files accessible, TSL with HTTP, BNA-TSL via HTTPS, using the TSL root store.
 * The sha2 files are not downloaded on initial update, but during updateTrustStoresOnDemand().
 */
TEST_F(TslManagerTest, DISABLED_downloadBNAWithTSLStore)
{
    EnvironmentVariableGuard gurad{ConfigurationKey::TSL_INITIAL_DOWNLOAD_URL,
                                   "http://epa-as-1-mock.erp-system.svc.cluster.local:80/TSL.xml"};
    auto requestSender = std::make_shared<UrlRequestSenderMock>(
        TlsCertificateVerifier::withCustomRootCertificates(""),
        std::chrono::seconds{
            Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS)},
        std::chrono::milliseconds{
            Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_RESOLVE_TIMEOUT_MILLISECONDS)});
    requestSender->setStrict(false);
    // Adding the OCSP response here requires the ocsp signer certificate to be part of the TSL
    requestSender->setOcspUrlRequestHandler(
        "http://ocsp-testref.tsl.telematik-test/ocsp",
        {{TslTestHelper::getTslSignerCertificate(), TslTestHelper::getTslSignerCACertificate(),
          MockOcsp::CertificateOcspTestMode::SUCCESS}},
        TslTestHelper::getDefaultOcspCertificate(), TslTestHelper::getDefaultOcspPrivateKey());
    auto manager = TslManager(std::move(requestSender), StaticData::getXmlValidator());
    ASSERT_NE(nullptr, manager.getTslTrustedCertificateStore(TslMode::TSL, std::nullopt).getStore());
    ASSERT_NE(nullptr, manager.getTslTrustedCertificateStore(TslMode::BNA, std::nullopt).getStore());
    manager.updateTrustStoresOnDemand();
}

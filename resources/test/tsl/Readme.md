`BNA_valid.xml` is the upstream `BNA-TSL.xml` which will occasionally expire, either the signer certificate or the XML signature or the `NextUpdate` value may become outdated.

In order to update the file, perform the steps as follows:

1. Download the latest BNA-TSL from https://download-testref.tsl.ti-dienste.de/P-BNetzA/Pseudo-BNetzA-VL.xml
2. Of the new BNA-TSL, extract the `ds:X509Certificate` field in the XML, check the issuer of the certificate and make sure the issuer is listed in `template_TSL_valid.xml` and `template_TSL_no_ocsp_mapping.xml`
3. If not, replace the `TSPService` entry in the TSL templates below the `CA for BNA-TSL` comment with the correct entry. The entry can be extracted from the upstream testref TSL https://download-ref.tsl.ti-dienste.de/ECC/ECC-RSA_TSL-ref.xml (see also A_17680).
4. If the TSL changed, update the tests, e.g. `expectedCertificates` in `TslManagerTest.cxx` may be outdated.

/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/tsl/C14NHelper.hxx"

#include <gtest/gtest.h>

#include "erp/util/Base64.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Hash.hxx"
#include "erp/util/String.hxx"
#include "erp/xml/XmlDocument.hxx"
#include "test_config.h"


class C14NHelperTest : public testing::Test
{
};

namespace {
    const std::string expectedC14NAssertion = R"---(<saml2:Assertion xmlns:saml2="urn:oasis:names:tc:SAML:2.0:assertion" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" ID="_75dfd787-64fd-4c20-abde-2bb0ab764685" IssueInstant="2020-05-25T16:27:50.661Z" Version="2.0" xsi:type="saml2:AssertionType"><saml2:Issuer>https://aktor-gateway.gematik.de/authn</saml2:Issuer><ds:Signature xmlns:ds="http://www.w3.org/2000/09/xmldsig#"><ds:SignedInfo><ds:CanonicalizationMethod Algorithm="http://www.w3.org/2001/10/xml-exc-c14n#"></ds:CanonicalizationMethod><ds:SignatureMethod Algorithm="http://www.w3.org/2007/05/xmldsig-more#sha256-rsa-MGF1"></ds:SignatureMethod><ds:Reference URI="#_75dfd787-64fd-4c20-abde-2bb0ab764685"><ds:Transforms><ds:Transform Algorithm="http://www.w3.org/2000/09/xmldsig#enveloped-signature"></ds:Transform><ds:Transform Algorithm="http://www.w3.org/2001/10/xml-exc-c14n#"><ec:InclusiveNamespaces xmlns:ec="http://www.w3.org/2001/10/xml-exc-c14n#" PrefixList="xsd"></ec:InclusiveNamespaces></ds:Transform></ds:Transforms><ds:DigestMethod Algorithm="http://www.w3.org/2001/04/xmlenc#sha256"></ds:DigestMethod><ds:DigestValue>pxUHWTtYgC5vhn5wCJHfdwWzaoKHoiJ6Sna1Hnm6ANA=</ds:DigestValue></ds:Reference></ds:SignedInfo><ds:SignatureValue>DMQglP2f0YFA/IN7SyQTDu+NtyqsfnPivT2RCs8XiB5YiE36LuNSUT81LSxL/vPUhfk7Ty73aap8aOirEDiHDe7OUeQiAv9qDsg0Nytl4YnhsoAZ2LMSje0rGOskBBO4E8gDzbdAcJn3BPCiTmO1PAvHBdM6fVeMVGUCPB4n/9eOUqseNqUlDxRZX0gsz+1JpdwKaMh2NKx0ed8GOMYf2tQpchZ5jD1hA5yKq/B9M5itEf/nPIDcGSFNJjtIeR3j3p+OEXWhIBOu6SCg0nSxxpqnZyr1EQshYgbT9ab2wXhI/IJUFCloflKYPwOeAjQSpEEM4h6fwyjKEQVPQjRxOw==</ds:SignatureValue><ds:KeyInfo><ds:X509Data><ds:X509Certificate>MIIEdjCCA16gAwIBAgIHA6amE6eqezANBgkqhkiG9w0BAQsFADCBhDELMAkGA1UEBhMCREUxHzAd
BgNVBAoMFmdlbWF0aWsgR21iSCBOT1QtVkFMSUQxMjAwBgNVBAsMKUtvbXBvbmVudGVuLUNBIGRl
ciBUZWxlbWF0aWtpbmZyYXN0cnVrdHVyMSAwHgYDVQQDDBdHRU0uS09NUC1DQTI0IFRFU1QtT05M
WTAeFw0xOTA1MjcyMjAwMDBaFw0yNDA1MjcyMTU5NTlaMHkxCzAJBgNVBAYTAkRFMSYwJAYDVQQK
DB1nZW1hdGlrIFRFU1QtT05MWSAtIE5PVC1WQUxJRDFCMEAGA1UEAww5QXV0aGVudGlzaWVydW5n
c2RpZW5zdCBmw7xyIGRpZSBUb2tlbnNpZ25hdHVyZW4gVEVTVC1PTkxZMIIBIjANBgkqhkiG9w0B
AQEFAAOCAQ8AMIIBCgKCAQEAtFke63eWFlL5dLq4/YLyHjMLOu/M82WM1SVCK8MpDJxCd2W9DYyy
KCgde+jF1NGoqjcp5kFu2q5OtmddHzXBrEnURKJF1b6kGzMKySfMpFfJuf0TCNEjh7Dk/LWb80lF
Zj/Mnb1KLUGNU5vECDoexexsYdF+Syi6Ffch1oPJ0X1jvmXvR77jINzYtvE120hskECGJ1hEG5Z3
7jq8egJPRcudzF2YafhWxvZgOYMcQO27h30dC/qY93niz2lA8ytSSzHWStXgYZolpTStTEFGt/to
39iV23JlSrzolTFEvKo37c0HPoCCFKxXm4PMwjBGP1ZTyUMmNTorXdp09alJzwIDAQABo4H2MIHz
MCEGA1UdIAQaMBgwCgYIKoIUAEwEgSMwCgYIKoIUAEwEgUswHQYDVR0OBBYEFDk+NZ5CR5LEEFW9
RBbHk51HkATGMB8GA1UdIwQYMBaAFD+2guJpWXnMOdLVUSeO4KZZCjGvMA4GA1UdDwEB/wQEAwIH
gDA2BgUrJAgDAwQtMCswKTAnMCUwIzAVDBNlUEEgQXV0aGVudGlzaWVydW5nMAoGCCqCFABMBIFM
MDgGCCsGAQUFBwEBBCwwKjAoBggrBgEFBQcwAYYcaHR0cDovL2VoY2EuZ2VtYXRpay5kZS9vY3Nw
LzAMBgNVHRMBAf8EAjAAMA0GCSqGSIb3DQEBCwUAA4IBAQAkuQPtyD/f3T+Jmhk9w072P6Pcnehh
95iEPqhSdeBfVTOefhWiEjCUaY5u44bZbq6JjaTvDTHtnfRE/xa7AEiyXi3YM8gF/fibJdXuI98u
rn3mhQ1ZrnCrIaYUciUR32Jwio9sIeNiHu67xtlTm8QsIi5nerZBmBh1ukFAOhlT8dvXXTWsGltE
L78vQL+7TUbRJffSRmEsAQHcZ2cpqB93kkdo3xilp7oal84FmV+m/+BBf3c5kaFEIVA7N3yNFQST
Gal/pC1TMc2uvz8AJF0z0luTyzgWgIEMv7RX8fPB02kv/qmUdOPjL3f0X7QT7zJ0Xxi/peYl/7MU
cWddl/A4</ds:X509Certificate></ds:X509Data></ds:KeyInfo></ds:Signature><saml2:Subject><saml2:NameID Format="urn:oasis:names:tc:SAML:1.1:nameid-format:X509SubjectName">CN=Adelheid Gräfin PopówitschTEST-ONLY, GIVENNAME=Adelheid Gräfin, SURNAME=Popówitsch, OU=109500969, OU=X110410317, O=Test GKV-SVNOT-VALID, C=DE</saml2:NameID><saml2:SubjectConfirmation Method="urn:oasis:names:tc:SAML:2.0:cm:bearer"></saml2:SubjectConfirmation></saml2:Subject><saml2:Conditions NotBefore="2020-05-25T16:27:50.661Z" NotOnOrAfter="2020-05-25T16:32:50.661Z"><saml2:AudienceRestriction><saml2:Audience>https://aktor-gateway.gematik.de</saml2:Audience></saml2:AudienceRestriction></saml2:Conditions><saml2:AuthnStatement AuthnInstant="2020-05-25T16:27:50.661Z"><saml2:AuthnContext><saml2:AuthnContextClassRef>urn:oasis:names:tc:SAML:2.0:ac:classes:SmartcardPKI</saml2:AuthnContextClassRef></saml2:AuthnContext></saml2:AuthnStatement><saml2:AttributeStatement><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/name" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">Adelheid Gräfin PopówitschTEST-ONLY</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/givenname" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">Adelheid Gräfin</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/surname" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">Popówitsch</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/country" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">DE</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/nameidentifier" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">X110410317</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="urn:gematik:subject:subject-id" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue><InstanceIdentifier xmlns="urn:hl7-org:v3" extension="X110410317" root="1.2.276.0.76.4.8"></InstanceIdentifier></saml2:AttributeValue></saml2:Attribute></saml2:AttributeStatement></saml2:Assertion>)---";

    // the ds-namespace must be inserted into SignedInfo by canonicalization
    const std::string expectedC14NSignedInfo = "<ds:SignedInfo xmlns:ds=\"http://www.w3.org/2000/09/xmldsig#\"><ds:CanonicalizationMethod Algorithm=\"http://www.w3.org/2001/10/xml-exc-c14n#\"></ds:CanonicalizationMethod><ds:SignatureMethod Algorithm=\"http://www.w3.org/2007/05/xmldsig-more#sha256-rsa-MGF1\"></ds:SignatureMethod><ds:Reference URI=\"#_75dfd787-64fd-4c20-abde-2bb0ab764685\"><ds:Transforms><ds:Transform Algorithm=\"http://www.w3.org/2000/09/xmldsig#enveloped-signature\"></ds:Transform><ds:Transform Algorithm=\"http://www.w3.org/2001/10/xml-exc-c14n#\"><ec:InclusiveNamespaces xmlns:ec=\"http://www.w3.org/2001/10/xml-exc-c14n#\" PrefixList=\"xsd\"></ec:InclusiveNamespaces></ds:Transform></ds:Transforms><ds:DigestMethod Algorithm=\"http://www.w3.org/2001/04/xmlenc#sha256\"></ds:DigestMethod><ds:DigestValue>pxUHWTtYgC5vhn5wCJHfdwWzaoKHoiJ6Sna1Hnm6ANA=</ds:DigestValue></ds:Reference></ds:SignedInfo>";

    const std::string resource_AktorAuthenticationAssertion_Adelheid_KVNR_xml = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/C14NTest/authenticationAssertion.xml");

constexpr const char* example1 = R"(<n0:local xmlns:n0="foo:bar" xmlns:n3="ftp://example.org"><n1:elem2 xmlns:n1="http://example.net" xml:lang="en"><n3:stuff xmlns:n3="ftp://example.org"/></n1:elem2></n0:local>)";
    constexpr const char* example2 = R"(<n2:pdu xmlns:n1="http://example.com" xmlns:n2="http://foo.example" xml:lang="fr" xml:space="preserve"><n1:elem2 xmlns:n1="http://example.net" xml:lang="en"><n3:stuff xmlns:n3="ftp://example.org"/></n1:elem2></n2:pdu>)";

    constexpr const char* expectedAssertionWithoutSignature = R"(<saml2:Assertion xmlns:saml2="urn:oasis:names:tc:SAML:2.0:assertion" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" ID="_75dfd787-64fd-4c20-abde-2bb0ab764685" IssueInstant="2020-05-25T16:27:50.661Z" Version="2.0" xsi:type="saml2:AssertionType"><saml2:Issuer>https://aktor-gateway.gematik.de/authn</saml2:Issuer><saml2:Subject><saml2:NameID Format="urn:oasis:names:tc:SAML:1.1:nameid-format:X509SubjectName">CN=Adelheid Gräfin PopówitschTEST-ONLY, GIVENNAME=Adelheid Gräfin, SURNAME=Popówitsch, OU=109500969, OU=X110410317, O=Test GKV-SVNOT-VALID, C=DE</saml2:NameID><saml2:SubjectConfirmation Method="urn:oasis:names:tc:SAML:2.0:cm:bearer"></saml2:SubjectConfirmation></saml2:Subject><saml2:Conditions NotBefore="2020-05-25T16:27:50.661Z" NotOnOrAfter="2020-05-25T16:32:50.661Z"><saml2:AudienceRestriction><saml2:Audience>https://aktor-gateway.gematik.de</saml2:Audience></saml2:AudienceRestriction></saml2:Conditions><saml2:AuthnStatement AuthnInstant="2020-05-25T16:27:50.661Z"><saml2:AuthnContext><saml2:AuthnContextClassRef>urn:oasis:names:tc:SAML:2.0:ac:classes:SmartcardPKI</saml2:AuthnContextClassRef></saml2:AuthnContext></saml2:AuthnStatement><saml2:AttributeStatement><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/name" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">Adelheid Gräfin PopówitschTEST-ONLY</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/givenname" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">Adelheid Gräfin</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/surname" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">Popówitsch</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/country" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">DE</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/nameidentifier" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">X110410317</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="urn:gematik:subject:subject-id" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue><InstanceIdentifier xmlns="urn:hl7-org:v3" extension="X110410317" root="1.2.276.0.76.4.8"></InstanceIdentifier></saml2:AttributeValue></saml2:Attribute></saml2:AttributeStatement></saml2:Assertion>)";
    constexpr const char* expectedAssertionWithoutSignatureWithXsd = R"(<saml2:Assertion xmlns:saml2="urn:oasis:names:tc:SAML:2.0:assertion" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" ID="_75dfd787-64fd-4c20-abde-2bb0ab764685" IssueInstant="2020-05-25T16:27:50.661Z" Version="2.0" xsi:type="saml2:AssertionType"><saml2:Issuer>https://aktor-gateway.gematik.de/authn</saml2:Issuer><saml2:Subject><saml2:NameID Format="urn:oasis:names:tc:SAML:1.1:nameid-format:X509SubjectName">CN=Adelheid Gräfin PopówitschTEST-ONLY, GIVENNAME=Adelheid Gräfin, SURNAME=Popówitsch, OU=109500969, OU=X110410317, O=Test GKV-SVNOT-VALID, C=DE</saml2:NameID><saml2:SubjectConfirmation Method="urn:oasis:names:tc:SAML:2.0:cm:bearer"></saml2:SubjectConfirmation></saml2:Subject><saml2:Conditions NotBefore="2020-05-25T16:27:50.661Z" NotOnOrAfter="2020-05-25T16:32:50.661Z"><saml2:AudienceRestriction><saml2:Audience>https://aktor-gateway.gematik.de</saml2:Audience></saml2:AudienceRestriction></saml2:Conditions><saml2:AuthnStatement AuthnInstant="2020-05-25T16:27:50.661Z"><saml2:AuthnContext><saml2:AuthnContextClassRef>urn:oasis:names:tc:SAML:2.0:ac:classes:SmartcardPKI</saml2:AuthnContextClassRef></saml2:AuthnContext></saml2:AuthnStatement><saml2:AttributeStatement><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/name" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">Adelheid Gräfin PopówitschTEST-ONLY</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/givenname" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">Adelheid Gräfin</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/surname" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">Popówitsch</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/country" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">DE</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="http://schemas.xmlsoap.org/ws/2005/05/identity/claims/nameidentifier" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue xsi:type="xsd:string">X110410317</saml2:AttributeValue></saml2:Attribute><saml2:Attribute Name="urn:gematik:subject:subject-id" NameFormat="urn:oasis:names:tc:SAML:2.0:attrname-format:uri"><saml2:AttributeValue><InstanceIdentifier xmlns="urn:hl7-org:v3" extension="X110410317" root="1.2.276.0.76.4.8"></InstanceIdentifier></saml2:AttributeValue></saml2:Attribute></saml2:AttributeStatement></saml2:Assertion>)";

    constexpr const char* request = R"---(<?xml version="1.0" encoding="UTF-8"?>
<env:Envelope xmlns:env="http://www.w3.org/2003/05/soap-envelope" xmlns:SOAP-ENC="http://www.w3.org/2003/05/soap-encoding" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:c14n="http://www.w3.org/2001/10/xml-exc-c14n#" xmlns:ds="http://www.w3.org/2000/09/xmldsig#" xmlns:saml1="urn:oasis:names:tc:SAML:1.0:assertion" xmlns:saml2="urn:oasis:names:tc:SAML:2.0:assertion" xmlns:xenc="http://www.w3.org/2001/04/xmlenc#" xmlns:wsc="http://docs.oasis-open.org/ws-sx/ws-secureconversation/200512" xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd" xmlns:chan="http://schemas.microsoft.com/ws/2005/02/duplex" xmlns:wsa5="http://www.w3.org/2005/08/addressing" xmlns:wsp="http://schemas.xmlsoap.org/ws/2004/09/policy" xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd" xmlns:wst="http://docs.oasis-open.org/ws-sx/ws-trust/200512" xmlns:hl7v3="urn:hl7-org:v3" xmlns:faphr="http://ws.gematik.de/fa/phr/v1.1" xmlns:favsdm="http://ws.gematik.de/fa/vsdm/vsd/v5.2" xmlns:rs="urn:oasis:names:tc:ebxml-regrep:xsd:rs:3.0" xmlns:lcm="urn:oasis:names:tc:ebxml-regrep:xsd:lcm:3.0" xmlns:err="http://ws.gematik.de/tel/error/v2.0" xmlns:phrext="http://ws.gematik.de/fa/phrext/v1.0" xmlns:dsmlc="urn:oasis:names:tc:DSML:2:0:core" xmlns:rim="urn:oasis:names:tc:ebxml-regrep:xsd:rim:3.0" xmlns:acc="http://ws.gematik.de/fd/phr/I_Account_Management/v1.0" xmlns:authentication="http://ws.gematik.de/fd/phrs/I_Authentication_Insurant/WSDL/v1.1" xmlns:authentication2="http://ws.gematik.de/fd/phrs/I_Authentication_Insurant/v1.1" xmlns:authorization="http://ws.gematik.de/fd/phrs/AuthorizationService/v1.1" xmlns:dmc="http://ws.gematik.de/fd/phr/I_Document_Management_Connect/v1.0" xmlns:dsml="http://ws.gematik.de/phr/dsml/v2" xmlns:query="urn:oasis:names:tc:ebxml-regrep:xsd:query:3.0" xmlns:rmd="urn:ihe:iti:rmd:2017" xmlns:xds="urn:ihe:iti:xds-b:2007">
    <env:Header>
        <wss:Security xmlns:wss="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd">##AUTHENTICATION-ASSERTION##</wss:Security>
        <wsa5:Action env:mustUnderstand="true">some action</wsa5:Action>
    </env:Header>
    <env:Body>
    </env:Body>
</env:Envelope>)---";

}


TEST_F(C14NHelperTest, testAssertion_RootC14N_ExclusiveXML10)
{
    const std::optional<std::string> signedDataResult = C14NHelper::generateC14N(
        resource_AktorAuthenticationAssertion_Adelheid_KVNR_xml,
        {},
        "http://www.w3.org/2001/10/xml-exc-c14n#");
    ASSERT_TRUE(signedDataResult.has_value());
    ASSERT_EQ(expectedC14NAssertion, *signedDataResult);
}


TEST_F(C14NHelperTest, testAssertion_AssertionC14N_ExclusiveXML10)
{
    const std::optional<std::string> signedDataResult = C14NHelper::generateC14N(
        resource_AktorAuthenticationAssertion_Adelheid_KVNR_xml,
        {
            {"Assertion", {"urn:oasis:names:tc:SAML:2.0:assertion"}}},
        "http://www.w3.org/2001/10/xml-exc-c14n#");
    ASSERT_TRUE(signedDataResult.has_value());
    ASSERT_EQ(expectedC14NAssertion, *signedDataResult);
}


TEST_F(C14NHelperTest, testAssertion_SignedInfoC14N_ExclusiveXML10)
{
    const std::optional<std::string> signedDataResult = C14NHelper::generateC14N(
        resource_AktorAuthenticationAssertion_Adelheid_KVNR_xml,
        {
            {"Assertion", {"urn:oasis:names:tc:SAML:2.0:assertion"}},
            {"Signature", {"http://www.w3.org/2000/09/xmldsig#"}},
            {"SignedInfo", {"http://www.w3.org/2000/09/xmldsig#"}}
        },
        "http://www.w3.org/2001/10/xml-exc-c14n#");
    ASSERT_TRUE(signedDataResult.has_value());
    ASSERT_EQ(expectedC14NSignedInfo, *signedDataResult);
}


TEST_F(C14NHelperTest, testAssertion_AssertionWithoutSignature)
{
    auto document = XmlDocument(resource_AktorAuthenticationAssertion_Adelheid_KVNR_xml);
    document.registerNamespace("saml2", "urn:oasis:names:tc:SAML:2.0:assertion");
    document.registerNamespace("ds", "http://www.w3.org/2000/09/xmldsig#");

    auto* signatureNode = document.getNode("/saml2:Assertion/ds:Signature");
    auto assertionWithoutSignature = C14NHelper::canonicalize(
        nullptr,       // include all of the document
        signatureNode, // except the signature
        &document.getDocument(),
        XML_C14N_EXCLUSIVE_1_0,
        {""},
        false);

    ASSERT_TRUE(assertionWithoutSignature.has_value());
    ASSERT_EQ(*assertionWithoutSignature, expectedAssertionWithoutSignature);
}


TEST_F(C14NHelperTest, canonicalize_assertionWithoutSignature_complex)
{
    std::string xml = String::replaceAll(request, "##AUTHENTICATION-ASSERTION##", resource_AktorAuthenticationAssertion_Adelheid_KVNR_xml);

    auto document = XmlDocument(xml);
    document.registerNamespace("env", "http://www.w3.org/2003/05/soap-envelope");
    document.registerNamespace("wss", "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd");
    document.registerNamespace("saml2", "urn:oasis:names:tc:SAML:2.0:assertion");
    document.registerNamespace("ds", "http://www.w3.org/2000/09/xmldsig#");

    auto assertionNode = document.getNode("/env:Envelope/env:Header/wss:Security/saml2:Assertion");
    auto signatureNode = document.getNode("/env:Envelope/env:Header/wss:Security/saml2:Assertion/ds:Signature");

    auto assertionWithoutSignature = C14NHelper::canonicalize(
        assertionNode,
        signatureNode,
        &document.getDocument(),
        XML_C14N_EXCLUSIVE_1_0,
        {"xsd"},
        false);

    ASSERT_TRUE(assertionWithoutSignature.has_value());
    EXPECT_EQ(*assertionWithoutSignature, expectedAssertionWithoutSignatureWithXsd);

    const std::string hash = ByteHelper::toHex(Hash::sha256(*assertionWithoutSignature));
    auto digestValue = document.getElementText("/env:Envelope/env:Header/wss:Security/saml2:Assertion/ds:Signature/ds:SignedInfo/ds:Reference/ds:DigestValue/text()");
    const std::string expectedHash = ByteHelper::toHex(Base64::decodeToString(digestValue));
    EXPECT_EQ(hash, expectedHash);
}


TEST_F(C14NHelperTest, testAssertion_SignedInfoC14N_XML10_Unsupported)
{
    const std::optional<std::string> signedDataResult = C14NHelper::generateC14N(
        resource_AktorAuthenticationAssertion_Adelheid_KVNR_xml,
        {
            {"Assertion", {"urn:oasis:names:tc:SAML:2.0:assertion"}},
            {"Signature", {"http://www.w3.org/2000/09/xmldsig#"}},
            {"SignedInfo", {"http://www.w3.org/2000/09/xmldsig#"}}
        },
        "http://www.w3.org/TR/2001/REC-xml-c14n-20010315");
    ASSERT_FALSE(signedDataResult.has_value());
}


// This test fails on iOS for unknown reasons. Activate the test when the failure has been analysed.
// The following test, ..._example2, still works and is similar, so disabling this test is not a huge loss.
TEST_F(C14NHelperTest, DISABLED_canonicalize_sc14n_example1)
{
    auto document1 = XmlDocument(example1);
    document1.registerNamespace("n0", "foo:bar");
    document1.registerNamespace("n1", "http://example.net");
    auto* elem2 = document1.getNode( "/n0:local/n1:elem2");
    auto canonicalizedSignedInfo = C14NHelper::canonicalize(
        elem2,
        nullptr,
        &document1.getDocument(),
        XML_C14N_EXCLUSIVE_1_0,
        {},
        false);

    ASSERT_TRUE(canonicalizedSignedInfo.has_value());
    ASSERT_EQ(*canonicalizedSignedInfo, R"(<n1:elem2 xmlns:n1="http://example.net" xml:lang="en"><n3:stuff xmlns:n3="ftp://example.org"></n3:stuff></n1:elem2>)");
}


TEST_F(C14NHelperTest, canonicalize_sc14n_example2)
{
    auto document1 = XmlDocument(example2);
    document1.registerNamespace("n2", "http://foo.example");
    document1.registerNamespace("n1", "http://example.net");
    auto* elem2 = document1.getNode( "/n2:pdu/n1:elem2");
    auto canonicalizedSignedInfo = C14NHelper::canonicalize(
        elem2,
        nullptr,
        &document1.getDocument(),
        XML_C14N_EXCLUSIVE_1_0,
        {},
        false);

    ASSERT_TRUE(canonicalizedSignedInfo.has_value());
    ASSERT_EQ(*canonicalizedSignedInfo, R"(<n1:elem2 xmlns:n1="http://example.net" xml:lang="en"><n3:stuff xmlns:n3="ftp://example.org"></n3:stuff></n1:elem2>)");
}

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

//
// Created by jens on 13.03.26.
//

#include "PoPPTokenBuilder.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/model/NumberAsStringParserWriter.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/util/SafeString.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"

#include <ctime>
PoPPTokenBuilder::PoPPTokenBuilder()
{
    mJson.SetObject();
    mJson.Parse(R"({
  "iat": 1722593256,
  "iss": "https://erp-mock-idp:8443",
  "proofMethod": "ehc-practitioner-trustedchannel-read-x509",
  "patientProofTime": 1722593255,
  "patientId": "X123456789",
  "insurerId": "123456789",
  "actorId": "1-2012345678",
  "actorProfessionOid": "1.2.276.0.76.4.50"
})");
    mJson["iat"] = std::time(nullptr);

    const SafeString prvPem{std::string{ResourceManager::instance().getStringResource(
        "test/generated_pki/sub_ca1_ec/certificates/popp_zd_sig_ec/popp_zd_sig_ec_key.pem")}};
    mPrivateKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(prvPem);
}
PoPPTokenBuilder& PoPPTokenBuilder::withTyp(const std::string& typ)
{
    mTyp = typ;
    return *this;
}
PoPPTokenBuilder& PoPPTokenBuilder::withAlg(JoseHeader::Algorithm alg)
{
    mAlg = alg;
    return *this;
}
PoPPTokenBuilder& PoPPTokenBuilder::withIat(int64_t iat)
{
    mJson["iat"] = iat;
    return *this;
}
PoPPTokenBuilder& PoPPTokenBuilder::withIss(const std::string& iss)
{
    mJson["iss"] = rapidjson::StringRef(iss);
    return *this;
}
PoPPTokenBuilder& PoPPTokenBuilder::withActorId(const std::string& actorId)
{
    mJson["actorId"] = rapidjson::StringRef(actorId);
    return *this;
}
PoPPTokenBuilder& PoPPTokenBuilder::withProofMethod(const std::string& str)
{
    mJson["proofMethod"] = rapidjson::StringRef(str);
    return *this;
}
PoPPTokenBuilder& PoPPTokenBuilder::withPatientId(const std::string& patientId)
{
    mJson["patientId"] = rapidjson::StringRef(patientId);
    return *this;
}
PoPPToken PoPPTokenBuilder::getToken() const
{
    return getToken(mPrivateKey);
}
PoPPToken PoPPTokenBuilder::getToken(const shared_EVP_PKEY& privateKey) const
{
    JoseHeader header{mAlg};
    header.setKeyId("x_vW4LVDipvu8iUQ5alKsZLWtH6jh4eJ4c5offXtMV0").setType(mTyp);
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    Expect(mJson.Accept(writer), "Failed to serialize claim.");
    const Jws jws(header, buffer.GetString(), privateKey);
    return PoPPToken(jws.compactSerialized());
}
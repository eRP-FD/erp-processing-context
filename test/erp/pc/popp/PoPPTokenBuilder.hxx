/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "erp/pc/popp/PoPPToken.hxx"
#include "shared/crypto/Jws.hxx"

#include <string>


class PoPPTokenBuilder
{
public:
    PoPPTokenBuilder();

    PoPPTokenBuilder& withTyp(const std::string& typ);
    PoPPTokenBuilder& withAlg(JoseHeader::Algorithm alg);
    PoPPTokenBuilder& withIat(int64_t iat);
    PoPPTokenBuilder& withIss(const std::string& iss);
    PoPPTokenBuilder& withActorId(const std::string& actorId);
    PoPPTokenBuilder& withProofMethod(const std::string& str);
    PoPPTokenBuilder& withPatientId(const std::string& patientId);

    PoPPToken getToken() const;
    PoPPToken getToken(const shared_EVP_PKEY& privateKey) const;

private:
    std::string mTyp = "vnd.telematik.popp+jwt";
    JoseHeader::Algorithm mAlg = JoseHeader::Algorithm::ES256;
    rapidjson::Document mJson;
    shared_EVP_PKEY mPrivateKey;
};

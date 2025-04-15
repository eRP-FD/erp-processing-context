/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/VauAutTokenSigner.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/Jws.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/util/Base64.hxx"

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/writer.h>
#include <chrono>

namespace
{
const rapidjson::Pointer claimIat("/iat");
const rapidjson::Pointer claimChallenge("/challenge");

// GEMREQ-start A_25935#define-sub
constexpr std::string_view payloadTemplate = R"--({
  "type":"ePA-Authentisierung über PKI",
  "sub":"9-E-Rezept-Fachdienst"
})--";
}// namespace
// GEMREQ-end A_25935#define-sub

VauAutTokenSigner::VauAutTokenSigner()
{
    rapidjson::MemoryStream s(payloadTemplate.data(), payloadTemplate.size());
    mPayloadDocument.ParseStream(s);
    ErpExpect(! mPayloadDocument.HasParseError(), HttpStatus::BadRequest, "text is not valid JSON");
}

// GEMREQ-start A_24771#sign
std::string VauAutTokenSigner::signAuthorizationToken(HsmSession& hsmSession, const std::string& freshness)
{
    using namespace std::chrono_literals;
    A_25165_03.start("Generate AUT-Certificate signed Bearer token");
    // GEMREQ-start A_25935#sign
    const auto header = JoseHeader(JoseHeader::Algorithm::ES256)
                            .setType("JWT")
                            .setX509Certificate(hsmSession.getVauAutCertificate(), false);

    // generate the payload based on the template
    const auto now = std::chrono::system_clock::now();
    const auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    claimIat.Set(mPayloadDocument, iat.count(), mPayloadDocument.GetAllocator());
    claimChallenge.Set(mPayloadDocument, freshness, mPayloadDocument.GetAllocator());
    rapidjson::StringBuffer payloadBuffer;
    rapidjson::Writer writer(payloadBuffer);
    mPayloadDocument.Accept(writer);
    const std::string payloadToBeSigned =
        header.serializeToBase64Url() + "." + Base64::toBase64Url(Base64::encode(payloadBuffer.GetString()));

    const auto signature = hsmSession.signWithVauAutKey(ErpVector::create(payloadToBeSigned));
    const auto signatureB64 = Base64::toBase64Url(Base64::encode(signature));
    // GEMREQ-end A_25935#sign
    A_25165_03.finish();

    return payloadToBeSigned + "." + signatureB64;
}
// GEMREQ-end A_24771#sign

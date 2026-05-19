/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/pc/popp/PoPPVerifier.hxx"
#include "PoPPCertificateVerifierService.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"

#include <fmt/format.h>


PoPPVerifier::PoPPVerifier()
    : mIatMaxAgeSeconds(Configuration::instance().poppTokenIatMaxAge())
{
}

// GEMREQ-start A_26450
void PoPPVerifier::verifyTokenSignature(const IPoPPCertificateVerifierService& poppService,
                                        const PoPPToken& poppToken)
{
    A_26450.start("Folgende Header-Attribute müssen im signierten JWT enthalten sein");
    ModelExpect(poppToken.typ() == PoPPToken::typHeader,
                fmt::format("Unexpected PoPP token type: {}", poppToken.typ()));
    ModelExpect(poppToken.alg() == PoPPToken::algHeader,
                fmt::format("Unexpected PoPP token algorithm: {}", poppToken.alg()));
    const std::string kid{poppToken.kid()};
    ModelExpect(! kid.empty(), "Key-ID in PoPP Token is empty");

    const auto poppKey = poppService.getKey(kid);
    ModelExpect(poppKey.has_value() && poppKey->publicKey.isSet(),
                fmt::format("Could not retrieve public key for kid: {}", kid));

    try
    {
        poppToken.verifySignature(poppKey->publicKey);
    }
    catch (const std::runtime_error& re)
    {
        ModelFail(re.what());
    }

    A_26450.finish();
}
//GEMREQ-end A_26450

// GEMREQ-start A_26452
void PoPPVerifier::verifyTokenClaims(const PoPPToken& poppToken,
                                     const PoPPCertificateVerifier::EntityStatement& entityStatement,
                                     const JWT& accessToken) const
{
    A_26452.start("iss - muss die URL des PoPP-Service aus dem Entity Statement sub-Attribut enthalten");
    ModelExpect(
        poppToken.iss() == entityStatement.sub(),
        fmt::format(
            "PoPPToken.iss must contain the URL of the PoPP-Service from entity statement sub attribute: {}!={}",
            poppToken.iss(), entityStatement.sub()));

    A_26452.start("iat - Ausstellungszeitpunkt des PoPP-Token muss anwendungsspezifisch geprüft werden (bspw. nicht "
                  "älter als 5 Minuten),");
    A_23399_01.start("Differenz zwischen Zeitstempel iat im Token und dem aktuellen Zeitpunkt nicht größer als 30 "
                     "Minuten (konfigurierbar)");
    const auto now = model::Timestamp::now();
    ModelExpect(
        now - poppToken.iat() < mIatMaxAgeSeconds,
        fmt::format("PoPPToken.iat is older than {} minutes",
                    std::chrono::duration_cast<std::chrono::minutes>(std::chrono::seconds(mIatMaxAgeSeconds)).count()));
    A_23399_01.finish();

    A_26452.start("actorId - Telematik-ID der LEI, für die der PoPP-Token ausgestellt wurde, muss abgeglichen werden, "
                  "gegen die vom PoPP-Verifier authentifizierte LEI, die den PoPP-Token vorlegt");
    A_23402_01.start("Prüfung PoPP-Token - Telematik-ID prüfen");
    const auto& accessTokenIdNumber = accessToken.stringForClaim(JWT::idNumberClaim);
    ModelExpect(accessTokenIdNumber.has_value(), "ACCESS_TOKEN does not contain the idNumber claim");
    ModelExpect(*accessTokenIdNumber == poppToken.actorId(), "PoPPToken.actorId does not match ACCESS_TOKEN.idNumber");

    A_26452.finish();
}
// GEMREQ-end A_26452

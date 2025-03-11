/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/tsl/TslProvider.hxx"
#include "shared/tsl/OcspService.hxx"
#include "shared/tsl/TslManager.hxx"
#include "shared/tsl/TslService.hxx"
#include "shared/tsl/X509Certificate.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Expect.hxx"

// GEMREQ-start A_16901
TslProvider& TslProvider::getInstance()
{
    static TslProvider instance;
    return instance;
}


TslProvider::TslProvider()
  : mMutex()
{
}

std::string TslProvider::getOcspResponse(const std::string& certAsDerBase64String)
{
    auto certificate = X509Certificate::createFromBase64(certAsDerBase64String);

    std::lock_guard lock(mMutex);

    Expect(mTslManager != nullptr, "TslProvider has no TslManager yet");

    auto response = mTslManager->getCertificateOcspResponse(
        TslMode::TSL,
        certificate,
        {},
        OcspCheckDescriptor{
            .mode = OcspCheckDescriptor::FORCE_OCSP_REQUEST_ALLOW_CACHE,
            .timeSettings =
                {.referenceTimePoint = std::nullopt, .gracePeriod = std::chrono::seconds{60}},
            .providedOcspResponse = nullptr

        });

    return response.response;
}

// GEMREQ-end A_16901

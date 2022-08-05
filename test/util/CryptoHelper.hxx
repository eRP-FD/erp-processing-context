/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef CRYPTOHELPER_H
#define CRYPTOHELPER_H

#include "erp/crypto/OpenSslHelper.hxx"
#include "fhirtools/model/Timestamp.hxx"

#include <optional>
#include <string>

namespace fhirtools{
class Timestamp;
}

namespace model
{
}
class CadesBesSignature;
class Certificate;

class CryptoHelper
{
public:
    static std::string toCadesBesSignature(const std::string& content,
                                           const std::optional<fhirtools::Timestamp>& signingTime = std::nullopt);
    static std::string qesCertificatePem();
    static Certificate cHpQes();
    static shared_EVP_PKEY cHpQesPrv();
    static Certificate cHpQesWansim();
    static shared_EVP_PKEY cHpQesPrvWansim();
};

#endif// CRYPTOHELPER_H

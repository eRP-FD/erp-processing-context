/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef CRYPTOHELPER_H
#define CRYPTOHELPER_H

#include "shared/crypto/OpenSslHelper.hxx"
#include "shared/model/Timestamp.hxx"

#include <optional>
#include <string>

class CadesBesSignature;
class Certificate;

class CryptoHelper
{
public:
    static std::string createCadesBesSignature(const std::string& pem_str,
                                            const std::string& content,
                                            const std::optional<model::Timestamp>& signingTime);
    static std::string toCadesBesSignature(const std::string& content,
                                           const std::optional<model::Timestamp>& signingTime = std::nullopt);
    static std::string toCadesBesSignature(const std::string& content,
                                           const std::string& cert_pem_filename,
                                           const std::optional<model::Timestamp>& signingTime = std::nullopt);
    static std::string qesCertificatePem(const std::optional<std::string>& cert_pem_filename = std::nullopt);
    static Certificate cHpQes();
    static shared_EVP_PKEY cHpQesPrv();
    static Certificate cQesG0();
    static shared_EVP_PKEY cQesG0Prv();
    static Certificate cHpQesWansim();
    static shared_EVP_PKEY cHpQesPrvWansim();
};

#endif// CRYPTOHELPER_H

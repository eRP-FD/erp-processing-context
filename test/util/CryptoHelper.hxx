/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef CRYPTOHELPER_H
#define CRYPTOHELPER_H

#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/model/Timestamp.hxx"

#include <optional>
#include <string>

class CadesBesSignature;
class Certificate;

class CryptoHelper
{
public:
    static std::string toCadesBesSignature(const std::string& content,
                                           const std::optional<model::Timestamp>& signingTime = std::nullopt);
    static std::string qesCertificatePem();
    static Certificate cHpQes();
    static shared_EVP_PKEY cHpQesPrv();
    static Certificate cQesG0();
    static shared_EVP_PKEY cQesG0Prv();
    static Certificate cHpQesWansim();
    static shared_EVP_PKEY cHpQesPrvWansim();
};

#endif// CRYPTOHELPER_H

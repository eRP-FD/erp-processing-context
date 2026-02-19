/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "shared/crypto/OpenSslHelper.hxx"

class X509Certificate;

class CrlProvider
{
public:
    virtual ~CrlProvider() = default;
    virtual shared_X509_CRL getCrl(const X509Certificate& cert) = 0;
};

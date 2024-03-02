/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/tsl/X509Store.hxx"

#include "erp/util/Expect.hxx"

#include <openssl/x509_vfy.h>

X509Store::X509Store()
    : mX509Store()
    , mStoredCertificates()
{
}

X509Store::X509Store(std::vector<X509Certificate> certificates)
    : mX509Store(X509_STORE_new(), X509_STORE_free)
    , mStoredCertificates(std::move(certificates))
{
    Expect(mX509Store != nullptr, "can not create X509_STORE object");
    for (X509Certificate& certificate : mStoredCertificates)
    {
        Expect(1 == X509_STORE_add_cert(mX509Store.get(), certificate.getX509Ptr()),
               "can not add certificate to X509_STORE");
    }
}

const std::vector<X509Certificate>& X509Store::getStoredCertificates() const
{
    return mStoredCertificates;
}

X509_STORE* X509Store::getStore() const
{
    return mX509Store.get();
}

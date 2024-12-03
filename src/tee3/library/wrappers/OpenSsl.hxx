/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_WRAPPERS_OPENSSL_HXX
#define EPA_LIBRARY_WRAPPERS_OPENSSL_HXX

#include "library/wrappers/detail/DisableWarnings.hxx"

DISABLE_WARNINGS_BEGIN

#include <openssl/bio.h>
// Temporarily move asn1t.h to second place to find out if e.g. bio.h is missing as well.
#include <openssl/asn1t.h>
#include <openssl/cmac.h>
#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/ocsp.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/tls1.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

DISABLE_WARNINGS_END

#endif

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef EGA_LIBRARY_UTIL_OPENSSL_HXX
#define EGA_LIBRARY_UTIL_OPENSSL_HXX

// OpenSSL headers contain several broken doc comments - disable clang's warnings about this.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif

#include <openssl/asn1t.h>
#include <openssl/bio.h>
#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/cmac.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/ocsp.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/pkcs12.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/tls1.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif

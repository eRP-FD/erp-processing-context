/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_CRYPTO_OPENSSLHELPER_HXX
#define ERP_CRYPTO_OPENSSLHELPER_HXX

#include <iostream>

#include "erp/util/Expect.hxx"
#include "erp/util/OpenSsl.hxx"

// Unique smart pointers for openssl types

template<class C, void(*deleter)(C*)>
class OpensslUniquePtr : public std::unique_ptr<C, decltype(deleter)>
{
public:
    OpensslUniquePtr()
        : std::unique_ptr<C, decltype(deleter)>(nullptr, deleter)
    {}

    explicit OpensslUniquePtr(C* pointer)
        : std::unique_ptr<C, decltype(deleter)>(pointer, deleter)
    {}
};

using Asn1ObjectPtr = OpensslUniquePtr<ASN1_OBJECT, &ASN1_OBJECT_free>;
using Asn1TypePtr = OpensslUniquePtr<ASN1_TYPE, &ASN1_TYPE_free>;
using OcspBasicRespPtr = OpensslUniquePtr<OCSP_BASICRESP, &OCSP_BASICRESP_free>;
using OcspCertidPtr = OpensslUniquePtr<OCSP_CERTID, &OCSP_CERTID_free>;
using OcspRequestPtr = OpensslUniquePtr<OCSP_REQUEST, &OCSP_REQUEST_free>;
using OcspResponsePtr = OpensslUniquePtr<OCSP_RESPONSE, &OCSP_RESPONSE_free>;
using EcdsaSignaturePtr = OpensslUniquePtr<ECDSA_SIG, &ECDSA_SIG_free>;


// Use
// #define LOCAL_LOGGING
// to activate logging of state changes and other operations on openssl_shared_ptr objects.

void showAllOpenSslErrors (void);
std::string bioToString (BIO* bio);
std::string x509NametoString(const X509_NAME* name);


/**
 * Initial attempt to make the openssl pointers as safe as possible. Many of them are C-style versions
 * of shared_ptr or unique_ptr.
 * This class tries to do that in a C++ way. There are some short cuts, mostly because the openssl functions
 * are not const correct.
 *
 * How to use:
 * - create new objects with the make() functions. Without argument creates a new object, otherwise takes
 *   ownership of the given pointer.
 * - conversion back to a raw pointer is done either via the conversion operator or explicitly via the get()
 *   function.
 */
template<
    class C,
    C*(*creator)(void),
    void(*deleter)(C*),
    int(*upref)(C*),
    const char* name=nullptr
    >
class openssl_shared_ptr
{
public:
    using self_t = openssl_shared_ptr<C,creator,deleter,upref,name>;

    /// Default create a new openssl_shared_ptr by calling the creator functor.
    static self_t make (void);
    /// Create a new openssl_shared_prt from the given raw pointer.
    static self_t make (C* p);

    openssl_shared_ptr (void) = default;
    openssl_shared_ptr(const self_t& other);
    openssl_shared_ptr(self_t&& other) noexcept;
    ~openssl_shared_ptr (void);

    void reset (void);

    self_t& operator= (self_t other) noexcept; // NOLINT(cppcoreguidelines-c-copy-assignment-signature,misc-unconventional-assign-operator)
    operator C* (void);
    operator const C* (void) const;
    C* get (void);
    C** getP (void);
    const C* get (void) const;
    C* operator-> (void);
    const C* operator-> (void) const;
    C* release (void);

    bool isSet (void) const;

    void swap(openssl_shared_ptr& other) noexcept;

    /// Use this method only when you "know" that a function does not modify the openssl_shared_ptr object
    /// but still requires a non-const pointer.
    openssl_shared_ptr& removeConst (void) const;

private:
    C* p{nullptr};

    // Don't expose this constructor to the public, so that we don't have to worry about increasing the reference
    // counter, i.e. expect that we are called only from make().
    explicit openssl_shared_ptr (C* q);
    std::ostream& log (void) const;
};

EVP_PKEY_CTX* new_EVP_PKEY_CTX (void);
void free_ASN1_SEQUENCE_ANY (ASN1_SEQUENCE_ANY* sequence);

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays)
extern const char asn1_bit_string_name[];
using shared_ASN1_BIT_STRING =     openssl_shared_ptr< ASN1_BIT_STRING,   ASN1_BIT_STRING_new,   ASN1_BIT_STRING_free,   nullptr,          asn1_bit_string_name>;

extern const char asn1_integer_name[];
using shared_ASN1_INTEGER =        openssl_shared_ptr< ASN1_INTEGER,      ASN1_INTEGER_new,      ASN1_INTEGER_free,      nullptr,          asn1_integer_name>;

extern const char asn1_object_name[];
using shared_ASN1_OBJECT =         openssl_shared_ptr< ASN1_OBJECT,       ASN1_OBJECT_new,       ASN1_OBJECT_free,       nullptr,          asn1_object_name>;

extern const char asn1_sequence_name[];
using shared_ASN1_SEQUENCE_ANY =   openssl_shared_ptr< ASN1_SEQUENCE_ANY, sk_ASN1_TYPE_new_null, free_ASN1_SEQUENCE_ANY, nullptr,        asn1_sequence_name>;

extern const char asn1_type_name[];
using shared_ASN1_TYPE =           openssl_shared_ptr< ASN1_TYPE,         ASN1_TYPE_new,         ASN1_TYPE_free,         nullptr,          asn1_type_name>;

extern const char bn_name[];
using shared_BN =                  openssl_shared_ptr< BIGNUM,            BN_new,                BN_free,                nullptr,          bn_name>;

extern const char bn_ctx_name[];
using shared_BN_CTX =              openssl_shared_ptr< BN_CTX,            BN_CTX_new,            BN_CTX_free,            nullptr,          bn_ctx_name>;

extern const char ec_group_name[];
using shared_EC_GROUP =            openssl_shared_ptr< EC_GROUP,          nullptr,               EC_GROUP_free,          nullptr,          ec_group_name>;

extern const char ec_key_name[];
using shared_EC_KEY =              openssl_shared_ptr< EC_KEY,            EC_KEY_new,            EC_KEY_free,            EC_KEY_up_ref,    ec_key_name>;

extern const char ec_point_name[];
using shared_EC_POINT =            openssl_shared_ptr< EC_POINT,          nullptr,               EC_POINT_free,          nullptr,          ec_point_name>;

extern const char evp_pkey_name[];
using shared_EVP_PKEY =            openssl_shared_ptr< EVP_PKEY,          EVP_PKEY_new,          EVP_PKEY_free,          EVP_PKEY_up_ref,  evp_pkey_name>;

extern const char evp_pkey_ctx_name[];
using shared_EVP_PKEY_CTX =        openssl_shared_ptr< EVP_PKEY_CTX,      new_EVP_PKEY_CTX,      EVP_PKEY_CTX_free,      nullptr,          evp_pkey_ctx_name>;

extern const char evp_md_ctx_name[];
using shared_EVP_MD_CTX =          openssl_shared_ptr< EVP_MD_CTX,        EVP_MD_CTX_new,        EVP_MD_CTX_free,        nullptr,          evp_md_ctx_name>;

extern const char ocsp_basic_response_name[];
using shared_OCSP_BASICRESP =      openssl_shared_ptr< OCSP_BASICRESP,    OCSP_BASICRESP_new,    OCSP_BASICRESP_free,    nullptr,          ocsp_basic_response_name>;

extern const char ocsp_response_name[];
using shared_OCSP_RESPONSE =       openssl_shared_ptr< OCSP_RESPONSE,     OCSP_RESPONSE_new,     OCSP_RESPONSE_free,     nullptr,          ocsp_response_name>;

extern const char rsa_name[];
using shared_RSA =                 openssl_shared_ptr< RSA,               RSA_new,               RSA_free,               RSA_up_ref,       rsa_name>;

extern const char x509_name[];
using shared_X509 =                openssl_shared_ptr< X509,              X509_new,              X509_free,              X509_up_ref,      x509_name>;

extern const char x509_Name_name[];
using shared_X509_Name =           openssl_shared_ptr< X509_NAME,         X509_NAME_new,         X509_NAME_free,         nullptr,          x509_Name_name>;

extern const char x509_Store_name[];
using shared_X509_Store =          openssl_shared_ptr< X509_STORE,        X509_STORE_new,        X509_STORE_free,       X509_STORE_up_ref, x509_Store_name>;

extern const char ecdsa_sig_name[];
using shared_ECDSA_SIG =           openssl_shared_ptr< ECDSA_SIG,         ECDSA_SIG_new,         ECDSA_SIG_free,         nullptr,          ecdsa_sig_name>;

extern const char pkcs7_name[];
using shared_PKCS7 =               openssl_shared_ptr< PKCS7,             PKCS7_new,             PKCS7_free,             nullptr,          pkcs7_name>;

extern const char cms_name[];
using shared_CMS_ContentInfo =     openssl_shared_ptr< CMS_ContentInfo,   CMS_ContentInfo_new,   CMS_ContentInfo_free,   nullptr,          cms_name>;

extern const char bio_name[];
// Adapt the new and free functions to conform to what openssl_shared_ptr<> expects.
BIO* createBIO (void);
void freeBIO (BIO* bio);
using shared_BIO = openssl_shared_ptr<BIO, createBIO,freeBIO, BIO_up_ref, bio_name>;

/**
 * Convert a BIGNUM into a std::string without loosing ownership of the temporarily and dynamically allocated char*.
 */
std::string BN_to_hexString (const shared_BN& value);


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
openssl_shared_ptr<C,creator,deleter,upref,name> openssl_shared_ptr<C,creator,deleter,upref,name>
    ::make (void)
{
    static_assert(creator!=nullptr);
    static_assert(deleter!=nullptr);

    self_t object (creator());
    Expect(object.isSet(), "could not created openSSL object");
    return object;
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
openssl_shared_ptr<C,creator,deleter,upref,name> openssl_shared_ptr<C,creator,deleter,upref,name>
    ::make (C* p_)
{
    static_assert(deleter!=nullptr);

    return self_t(p_);
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
openssl_shared_ptr<C,creator,deleter,upref,name>
::openssl_shared_ptr (C* q)
    : p(q)
{
#ifdef LOCAL_LOGGING
    log() << "constructor(*): created with p " << (void*)p << '\n';
#endif
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
openssl_shared_ptr<C,creator,deleter,upref,name>
    ::openssl_shared_ptr(const self_t& other)
        : p(other.p)
{
    static_assert(upref != nullptr);
    if (p)
    {
        upref(p);
#ifdef LOCAL_LOGGING
        log() << "constructor(&): increased ref count\n";
#endif
    }
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
openssl_shared_ptr<C,creator,deleter,upref,name>
    ::openssl_shared_ptr(self_t&& other) noexcept
{
    swap(other);
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
openssl_shared_ptr<C,creator,deleter,upref,name>
    ::~openssl_shared_ptr (void)
{
    reset();
}

template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
void openssl_shared_ptr<C,creator,deleter,upref,name>
    ::reset (void)
{
    if (p != nullptr)
    {
#ifdef LOCAL_LOGGING
        log() << "reset: deleting " << (void*)p << '\n';
#endif
        deleter(p);
        p = nullptr;
    }
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
openssl_shared_ptr<C,creator,deleter,upref,name>& openssl_shared_ptr<C,creator,deleter,upref,name>    // NOLINT(cppcoreguidelines-c-copy-assignment-signature,misc-unconventional-assign-operator)
    ::operator= (self_t other) noexcept
{
    swap(other);
    return *this;
}


template<class C, C* (*creator)(void), void (*deleter)(C*), int (*upref)(C*), const char* name>
void openssl_shared_ptr<C, creator, deleter, upref, name>::swap(
    openssl_shared_ptr<C, creator, deleter, upref, name>& other) noexcept
{
    std::swap(p, other.p);
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
openssl_shared_ptr<C,creator,deleter,upref,name>
    ::operator C* (void)
{
#ifdef LOCAL_LOGGING
    log() << "operator C*: returning " << (void*)p << '\n';
#endif
    return p;
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
openssl_shared_ptr<C,creator,deleter,upref,name>
   ::operator const C* (void) const
{
#ifdef LOCAL_LOGGING
    log() << "operator const C*: returning " << (void*)p << '\n';
#endif
    return p;
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
C* openssl_shared_ptr<C,creator,deleter,upref,name>
    ::get (void)
{
#ifdef LOCAL_LOGGING
    log() << "get: returning " << (void*)p << '\n';
#endif
    return p;
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
C* openssl_shared_ptr<C,creator,deleter,upref,name>
    ::release (void)
{
#ifdef LOCAL_LOGGING
    log() << "release: returning " << (void*)p << '\n';
#endif
    C* result = p;
    p = nullptr;
    return result;
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
C** openssl_shared_ptr<C,creator,deleter,upref,name>
    ::getP (void)
{
#ifdef LOCAL_LOGGING
    log() << "getP: returning " << (void*)&p << '\n';
#endif
    return &p;
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
const C* openssl_shared_ptr<C,creator,deleter,upref,name>
    ::get (void) const
{
#ifdef LOCAL_LOGGING
    log() << "const get: returning " << (void*)p << '\n';
#endif
    return p;
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
C* openssl_shared_ptr<C,creator,deleter,upref,name>
    ::operator-> (void)
{
#ifdef LOCAL_LOGGING
    log() << "operator ->: returning " << (void*)p << '\n';
#endif
    return p;
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
const C* openssl_shared_ptr<C,creator,deleter,upref,name>
    ::operator-> (void) const
{
#ifdef LOCAL_LOGGING
    log() << "operator -> const: returning " << (void*)p << '\n';
#endif
    return p;
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
std::ostream& openssl_shared_ptr<C,creator,deleter,upref,name>
    ::log (void) const
{
    if (name != nullptr)
        return std::cerr << name << " : ";
    else
        return std::cerr;
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
bool openssl_shared_ptr<C,creator,deleter,upref,name>
    ::isSet (void) const
{
    return p!= nullptr;
}


template<class C, C*(*creator)(void), void(*deleter)(C*), int(*upref)(C*), const char* name>
openssl_shared_ptr<C,creator,deleter,upref,name>& openssl_shared_ptr<C,creator,deleter,upref,name>
    ::removeConst (void) const
{
    return const_cast<self_t&>(*this); // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

// NOLINTEND(cppcoreguidelines-avoid-c-arrays)

#endif

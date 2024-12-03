/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/crypto/OpenSslHelper.hxx"
#include "shared/crypto/OpenSsl.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/String.hxx"
#include "shared/util/TLog.hxx"


// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays)
extern const char asn1_bit_string_name[] = "ASN1_BIT_STRING";
extern const char asn1_integer_name[] = "ASN1_INTEGER";
extern const char asn1_object_name[] = "ASN1_OBJECT";
extern const char asn1_sequence_name[] = "ASN1_SEQUENCE_ANY";
extern const char asn1_type_name[] = "ASN1_TYPE";
extern const char bio_name[] = "BIO";
extern const char bn_ctx_name[] = "BN_CTX";
extern const char bn_name[] = "BIGNUM";
extern const char ec_key_name[] = "EC_KEY";
extern const char ec_point_name[] = "EC_POINT";
extern const char ec_group_name[] = "EC_GROUP";
extern const char evp_md_ctx_name[] = "EVP_MD_CTX";
extern const char evp_pkey_name[] = "EVP_PKEY";
extern const char evp_pkey_ctx_name[] = "EVP_PKEY_CTX";
extern const char ocsp_basic_response_name[] = "OCSP_BASICRESP";
extern const char ocsp_response_name[] = "OCSP_RESPONSE";
extern const char x509_name[] = "X509";
extern const char x509_Name_name[] = "X509_NAME";
extern const char x509_Store_name[] = "X509_STORE";
extern const char ecdsa_sig_name[] = "ECDSA_SIG";
extern const char cms_name[] = "CMS_ContentInfo";
// NOLINTEND(cppcoreguidelines-avoid-c-arrays)


void opensslBufferDeleteFunction(unsigned char* pointer)
{
    OPENSSL_free(pointer);
}

void showAllOpenSslErrors (void)
{
    while(true)
    {
        const unsigned long code = ERR_get_error();
        if (code == 0)
            break;
        std::array<char, 1024> buffer{};
        ERR_error_string_n(code, buffer.data(), buffer.size());
        TLOG(ERROR) << std::string(buffer.begin(), buffer.end());
    }
}


BIO* createBIO (void)
{
    return BIO_new(BIO_s_mem());
}


void freeBIO (BIO* bio)
{
    BIO_free(bio);
}


std::string bioToString (BIO* bio)
{
    char* data = nullptr;
    const auto length = BIO_get_mem_data(bio, &data);
    OpenSslExpect(length >= 0, "BIO_get_mem_data failed");
    return std::string(data, static_cast<size_t>(length));
}

std::string x509NametoString(const X509_NAME* name)
{
    auto mem = shared_BIO::make();
    const int status = X509_NAME_print(mem.get(), name, 0);
    if (status != 1)
    {
        showAllOpenSslErrors();
        std::string location = std::string(__FILE__) + ":" + std::to_string(__LINE__);
        Fail2("OpenSslHelper error at " + location + "\n    serialization of certificate failed", std::runtime_error);
    }
    return bioToString(mem.get());
}

EVP_PKEY_CTX* new_EVP_PKEY_CTX (void)
{
    return EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
}

ASN1_SEQUENCE_ANY* make_ASN1_SEQUENCE_ANY()
{
    return sk_ASN1_TYPE_new_null();
}


void free_ASN1_SEQUENCE_ANY (ASN1_SEQUENCE_ANY* sequence)
{
    sk_ASN1_TYPE_pop_free(sequence, ASN1_TYPE_free);
}


std::string BN_to_hexString (const shared_BN& value)
{
    char* hex = BN_bn2hex(value);
    std::string s = hex;
    OPENSSL_free(hex);
    return s;
}

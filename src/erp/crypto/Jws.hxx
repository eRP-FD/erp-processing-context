#ifndef ERP_PROCESSING_CONTEXT_JWS_HXX
#define ERP_PROCESSING_CONTEXT_JWS_HXX

#include <rapidjson/pointer.h>
#include <optional>
#include <string_view>
#include <vector>

#include "erp/common/MimeType.hxx"
#include "erp/crypto/Certificate.hxx"

class JoseHeader
{
public:
    enum class Algorithm
    {
        ES256,
        BP256R1
    };
    explicit JoseHeader(Algorithm algorithm);

    JoseHeader& setJwkSetUrl(const std::string_view& jwkSetUrl);
    JoseHeader& setJsonWebKey(const std::string_view& jsonWebKey);
    JoseHeader& setKeyId(const std::string_view& keyId);
    JoseHeader& setX509Url(const std::string_view& x509Url);
    JoseHeader& setX509Certificate(const Certificate& x509Certificate);
    JoseHeader& setType(const MimeType& type);
    JoseHeader& setContentType(const MimeType& contentType);
    JoseHeader& setCritical(const std::initializer_list<std::string>& critical);

    std::string serializeToBase64Url() const;

    const Algorithm algorithm;

private:
    struct HeaderField {
        const rapidjson::Pointer key;
        std::optional<std::string> value = {};
    };
    struct HeaderFieldArray {
        const rapidjson::Pointer key;
        std::vector<std::string> value = {};
    };

    HeaderField mAlgorithm{rapidjson::Pointer("/alg")};
    HeaderField mJwkSetUrl{rapidjson::Pointer("/jku")};
    HeaderField mJsonWebKey{rapidjson::Pointer("/jwk")};
    HeaderField mKeyId{rapidjson::Pointer("/kid")};
    HeaderField mX509Url{rapidjson::Pointer("/x5u")};
    HeaderFieldArray mX509Certificate{rapidjson::Pointer("/x5c")};
    HeaderField mX509CertificateSha1Thumbprint{rapidjson::Pointer("/x5t")};
    HeaderField mX509CertificateSha265Thumbprint{rapidjson::Pointer("/x5t#S256")};
    HeaderField mType{rapidjson::Pointer("/typ")};
    HeaderField mContentType{rapidjson::Pointer("/cty")};
    HeaderFieldArray mCritical{rapidjson::Pointer("/crit")};
};

/// @brief represents a RFC 7515 JSON Web Signature
class Jws
{
public:
    /// @brief sign the payload based on settings in header using private key.
    explicit Jws(const JoseHeader& header, std::string_view payload, const shared_EVP_PKEY& privateKey);

    /// @brief serialize to JWS Compact Serialization, jose.payload.signature
    std::string compactSerialized() const;

    /// @brief serialize to JWS Compact Serialization (detached), jose..signature
    std::string compactDetachedSerialized() const;

private:
    std::string mSerializedHeader;
    std::string mSerializedPayload;
    std::string mSerializedSignature;
};


#endif//ERP_PROCESSING_CONTEXT_JWS_HXX

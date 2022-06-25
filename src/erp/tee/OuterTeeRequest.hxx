/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TEE_OUTERTEEREQUEST_HXX
#define ERP_PROCESSING_CONTEXT_TEE_OUTERTEEREQUEST_HXX

#include <cstdint>
#include <string>

#include "erp/crypto/AesGcm.hxx"
#include "erp/crypto/EllipticCurve.hxx"


/**
 * A_20161, 6/g defines the components of the outer request body to be
 *     chr(0x01)
 *     32 byte x coordinate of public key
 *     32 byte y coordinate of public key
 *     12 byte IV
 *     AES/GCM encrypted request
 *     16 byte authentication tag
 */
class OuterTeeRequest
{
public:
    static constexpr size_t VersionLength = 1;

    uint8_t version{};
    uint8_t xComponent[EllipticCurve::KeyCoordinateLength]{};
    uint8_t yComponent[EllipticCurve::KeyCoordinateLength]{};
    uint8_t iv[AesGcm128::IvLength]{};
    std::string ciphertext;
    uint8_t authenticationTag[AesGcm128::AuthenticationTagLength]{};

    std::string assemble (void) const;
    static OuterTeeRequest disassemble (const std::string& body);

    // version is a single byte, no conversion to a string type required.
    std::string_view getPublicX (void) const;
    std::string_view getPublicY (void) const;
    std::string_view getIv (void) const;
    // ciphertext is already accessible as std::string
    std::string_view getAuthenticationTag (void) const;
};


#endif

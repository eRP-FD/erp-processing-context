/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/tee/OuterTeeRequest.hxx"

#include "erp/util/Expect.hxx"


namespace {
    constexpr size_t MessageLengthWithoutCiphertext =
        OuterTeeRequest::VersionLength
        + EllipticCurve::KeyCoordinateLength // x
        + EllipticCurve::KeyCoordinateLength // y
        + AesGcm128::IvLength
        + AesGcm128::AuthenticationTagLength;

    void copy (uint8_t* destination, const std::string& source, const size_t offset, const size_t length)
    {
        std::copy(source.data() + offset, source.data() + offset + length, destination);
    }
}


std::string OuterTeeRequest::assemble (void) const
{
    std::string result (MessageLengthWithoutCiphertext + ciphertext.size(), 'X');

    result[0] = static_cast<char>(version);
    size_t offset = VersionLength;
    std::copy(xComponent, xComponent + EllipticCurve::KeyCoordinateLength, result.data() + offset);
    offset += EllipticCurve::KeyCoordinateLength;
    std::copy(yComponent, yComponent + EllipticCurve::KeyCoordinateLength, result.data() + offset);
    offset += EllipticCurve::KeyCoordinateLength;
    std::copy(iv, iv+AesGcm128::IvLength, result.data() + offset);
    offset += AesGcm128::IvLength;
    std::copy(ciphertext.begin(), ciphertext.end(), result.data() + offset);
    offset += ciphertext.size();
    std::copy(authenticationTag, authenticationTag+AesGcm128::AuthenticationTagLength, result.data() + offset);

    return result;
}


OuterTeeRequest OuterTeeRequest::disassemble (const std::string& outerRequestBody)
{
    ErpExpect(outerRequestBody.size() >= MessageLengthWithoutCiphertext, HttpStatus::BadRequest,
              "body is too short to be a outer request body");
    OuterTeeRequest message;

    message.version = gsl::narrow<uint8_t>(outerRequestBody[0]);
    size_t offset = VersionLength;
    ::copy(message.xComponent, outerRequestBody, offset, EllipticCurve::KeyCoordinateLength);
    offset += EllipticCurve::KeyCoordinateLength;
    ::copy(message.yComponent, outerRequestBody, offset, EllipticCurve::KeyCoordinateLength);
    offset += EllipticCurve::KeyCoordinateLength;
    ::copy(message.iv, outerRequestBody, offset, AesGcm128::IvLength);
    offset += AesGcm128::IvLength;
    const size_t cipherTextLength = outerRequestBody.size() - MessageLengthWithoutCiphertext;
    message.ciphertext = outerRequestBody.substr(offset, cipherTextLength);
    offset += cipherTextLength;
    ::copy(message.authenticationTag, outerRequestBody, offset, AesGcm128::AuthenticationTagLength);

    return message;
}


std::string_view OuterTeeRequest::getPublicX (void) const
{
    return {reinterpret_cast<const char*>(xComponent), EllipticCurve::KeyCoordinateLength};
}


std::string_view OuterTeeRequest::getPublicY (void) const
{
    return {reinterpret_cast<const char*>(yComponent), EllipticCurve::KeyCoordinateLength};
}


std::string_view OuterTeeRequest::getIv (void) const
{
    return {reinterpret_cast<const char*>(iv), AesGcm128::IvLength};
}


std::string_view OuterTeeRequest::getAuthenticationTag (void) const
{
    return {reinterpret_cast<const char*>(authenticationTag), AesGcm128::AuthenticationTagLength};
}

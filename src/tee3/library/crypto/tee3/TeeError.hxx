/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_TEEERROR_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_TEEERROR_HXX

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "shared/network/message//HttpStatus.hxx"

namespace epa
{

/**
 * Represent error messages according to A_24635.
 */
class TeeError : public std::runtime_error
{
public:
    enum class Code : uint8_t
    {
        InternalServerError = 0,

        DecodingError = 1,
        MissingParameters = 2,
        GcmDecryptionFailure = 3,
        PuNonPuFailure = 4,
        TransscriptError = 5,
        BadFormatExtendedCiphertext = 6,
        NotARequest = 7,
        UnknownKeyId = 8,
        UnknownCid = 9,

        FailedPublicKeyVerification = 10,
        __NotDefined = 11 //NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
    };


    /**
     * TeeError can only be serialized, not deserialized.
     */
    template<typename Processor>
    constexpr void processMembers(Processor& processor) const
    {
        const auto& error = descriptionForErrorCode(mErrorCode);

        processor("/MessageType", std::string_view("Error"));
        processor("/ErrorCode", mErrorCode);
        processor("/ErrorMessage", error.message);

        // A_24635: Additional values are allowed.
        processor("/Description", error.description);
        processor("/Details", std::string_view(what()));
    }

    TeeError(Code error, const std::string& message);

    Code code() const;
    HttpStatus httpStatus() const;

private:
    Code mErrorCode;

    struct ErrorDescription
    {
        HttpStatus status;
        Code code;
        std::string_view message;
        std::string_view description;
    };
    const static std::vector<ErrorDescription> mErrorDescriptions;

    static const ErrorDescription& descriptionForErrorCode(Code error);
};

} // namespace epa

#endif

/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee3/TeeError.hxx"
#include "library/log/Log.hxx"

namespace epa
{

const std::vector<TeeError::ErrorDescription> TeeError::mErrorDescriptions = {
    TeeError::ErrorDescription{
        HttpStatus::InternalServerError,
        TeeError::Code::InternalServerError,
        "Internal Server Error",
        "Unklassifizierter Server Error"},
    TeeError::ErrorDescription{
        HttpStatus::BadRequest,
        TeeError::Code::DecodingError,
        "Decoding Error",
        "Fehler in einer Kodierung bspw. der CBOR-Kodierung"},
    TeeError::ErrorDescription{
        HttpStatus::BadRequest,
        TeeError::Code::MissingParameters,
        "Missing Parameters",
        "notwendige Datenfelder bspw. in der Handshake fehlen"},
    TeeError::ErrorDescription{
        HttpStatus::Forbidden,
        TeeError::Code::GcmDecryptionFailure,
        "GCM returns FAIL",
        "Eine AES/GCM-Entschlüsselung ergibt das Symbol \"FAIL\"."},
    TeeError::ErrorDescription{
        HttpStatus::Forbidden,
        TeeError::Code::PuNonPuFailure,
        "PU/nonPU Failure",
        ""},
    TeeError::ErrorDescription{
        HttpStatus::Forbidden,
        TeeError::Code::TransscriptError,
        "Transscript Error",
        "Im Handshake wird Ungleichheit zwischen den beiden Transskript-Hashwerten festgestellt."},
    TeeError::ErrorDescription{
        HttpStatus::BadRequest,
        TeeError::Code::BadFormatExtendedCiphertext,
        "bad format: extended ciphertext",
        "Die erste Sanity-Prüfung des erweiterten Chiffrat bspw. bei A_24630 schlägt fehl."},
    TeeError::ErrorDescription{
        HttpStatus::Forbidden,
        TeeError::Code::NotARequest,
        "is not a request",
        "A_24630 (Prüfschritt 3)"},
    TeeError::ErrorDescription{
        HttpStatus::Forbidden,
        TeeError::Code::UnknownKeyId,
        "unknow KeyID",
        ""},
    TeeError::ErrorDescription{
        HttpStatus::Forbidden,
        TeeError::Code::UnknownCid,
        "unknown CID",
        ""},
    TeeError::ErrorDescription{
        HttpStatus::BadRequest,
        TeeError::Code::FailedPublicKeyVerification,
        "verification of public keys failed",
        "Die Verifikation der Public Keys (M1) ist fehl geschlagen"}};


TeeError::TeeError(Code error, const std::string& message)
  : std::runtime_error(message),
    mErrorCode(error)
{
}


TeeError::Code TeeError::code() const
{
    return mErrorCode;
}


HttpStatus TeeError::httpStatus() const
{
    const auto& descryption = descriptionForErrorCode(mErrorCode);
    return descryption.status;
}


const TeeError::ErrorDescription& TeeError::descriptionForErrorCode(const Code error)
{
    if (error == Code::InternalServerError || error >= Code::__NotDefined)
        return mErrorDescriptions[0];
    else
    {
        const auto& description = mErrorDescriptions[static_cast<size_t>(error)];
        if (description.code == error)
            return description;
        else
        {
            LOG(ERROR) << "detected inconsistency in TeeError descriptions for code "
                       << static_cast<uint8_t>(error);
            return mErrorDescriptions[0];
        }
    }
}

} // namespace epa

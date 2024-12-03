/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_CBOR_CBORERROR_HXX
#define EPA_LIBRARY_UTIL_CBOR_CBORERROR_HXX

#include <stdexcept>

namespace epa
{

class CborError : public std::runtime_error
{
public:
    enum class Code
    {
        DeserializationError,
        DeserializationMissingItem,
        SerializationError,
        UnsupportedFeature
    };

    CborError(Code code, const std::string& text);

    Code code() const;

private:
    const Code mCode;
};
} // namespace epa

#endif

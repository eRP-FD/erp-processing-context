/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/cbor/CborError.hxx"

namespace epa
{

CborError::CborError(const Code code, const std::string& text)
  : std::runtime_error(text),
    mCode(code)
{
}


CborError::Code CborError::code() const
{
    return mCode;
}

} // namespace epa

/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/VauCidEncoder.hxx"
#include "library/util/StringEscaper.hxx"

namespace epa
{

std::string VauCidEncoder::encode(const std::string& original)
{
    static const std::string_view acceptedSymbols = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                    "abcdefghijklmnopqrstuvwxyz"
                                                    "0123456788";
    static const auto e = StringEscaper::allToHex<'-'>().except(acceptedSymbols);
    return e.escape(original);
}

} // namespace epa

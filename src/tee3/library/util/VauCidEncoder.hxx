/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_VAUCIDENCODER_HXX
#define EPA_LIBRARY_UTIL_VAUCIDENCODER_HXX

#include <string>

namespace epa
{

/**
 * The class is responsible to encode VAU-CID segments.
 * According to A_24622 the returned value must only contains characters "a-zA-Z0-9-/"
 * Symbol '-' is used for encoding, thus it must be encoded as well. And "/" is used to
 * separate segments, thus it must be encoded too inside a segment.
 *
 * As result the accepted symbols that are not encoded are "a-zA-Z0-9".
 *
 */
class VauCidEncoder
{
public:
    static std::string encode(const std::string& original);
};

} // namespace epa

#endif

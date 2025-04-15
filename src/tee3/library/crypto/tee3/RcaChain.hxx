/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_RCACHAIN_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_RCACHAIN_HXX

#include <string>

namespace epa
{

/**
 * Load chains of RCA<N> certificates from the `resources` directory.
 * Each chain is rooted in RCA5 and contains cross signed certificates from RCA5 to the
 * end certificate RCA<N> with N>=5.
 */
class RcaChain
{
public:
    static std::string_view getRcaChain(const size_t n);

    static std::string_view getTestCa();
    static std::string_view getTestRcaChain();
};

} // namespace epa

#endif

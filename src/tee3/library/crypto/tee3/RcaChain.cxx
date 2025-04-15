/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee3/RcaChain.hxx"
#include "library/util/Assert.hxx"

#include "rca-chains.hxx"

namespace epa
{

namespace
{
    constexpr std::string_view beginCertificate = "-----BEGIN CERTIFICATE-----";

    /**
     * Remove the first certificate from the given PEM chain of certificates.
     * This first certificate is expected to be RCA5, which all clients should provide
     * themselves.
     */
    constexpr std::string_view stripRca5(const std::string_view pem)
    {
        const auto firstCertificate = pem.find(beginCertificate);
        Assert(firstCertificate != std::string_view::npos) << "PEM certificate not found";
        const auto secondCertificate =
            pem.find(beginCertificate, firstCertificate + beginCertificate.size());
        if (secondCertificate == std::string_view::npos)
            return "";
        else
            return pem.substr(secondCertificate);
    }
}


std::string_view RcaChain::getRcaChain(const size_t n)
{
    switch (n)
    {
        case 5:
            return stripRca5(resource_rca5chain_pem);
        case 6:
            return stripRca5(resource_rca6chain_pem);
        case 7:
            return stripRca5(resource_rca7chain_pem);
        case 8:
            return stripRca5(resource_rca8chain_pem);
        default:
            Failure() << "there is no certificate chain for RCA" << n;
    }
}


std::string_view RcaChain::getTestCa()
{
#ifdef EPA_BUILD_IS_PU
    Failure() << "test data not available in production builds";
#else
    return resource_aclos_fd_ca1_pem;
#endif
}


std::string_view RcaChain::getTestRcaChain()
{
#ifdef EPA_BUILD_IS_PU
    Failure() << "test data not available in production builds";
#else
    return resource_aclos_rca_pem;
#endif
}

} // namespace epa

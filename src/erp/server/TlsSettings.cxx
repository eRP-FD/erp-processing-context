/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/server/TlsSettings.hxx"

#include <boost/beast/core/error.hpp>


namespace
{
    std::string getCyphers(const std::optional<std::string>& forcedCiphers)
    {
        if (forcedCiphers.has_value())
        {
            return *forcedCiphers;
        }

        return "ECDHE-ECDSA-AES256-GCM-SHA384:"
               "ECDHE-ECDSA-AES128-GCM-SHA256:"
               "ECDHE-RSA-AES256-GCM-SHA384:"
               "ECDHE-RSA-AES128-GCM-SHA256";
    }
}

void TlsSettings::restrictVersions(boost::asio::ssl::context &sslContext)
{
    /* Prevent fall-back attacks. */
    // GEMREQ-start GS-A_5035
    sslContext.set_options(boost::asio::ssl::context::no_sslv2 |
                           boost::asio::ssl::context::no_sslv3 |
                           boost::asio::ssl::context::no_tlsv1 |
                           boost::asio::ssl::context::no_tlsv1_1);
    // GEMREQ-end GS-A_5035
}

void TlsSettings::setAllowedCiphersAndCurves(boost::asio::ssl::context& sslContext,
                                             const std::optional<std::string>& forcedCiphers)
{
    /* Set allowed cipher suites and curves. */
    // GEMREQ-start A_15751
    // GEMREQ-start A_17124
    if (1 != SSL_CTX_set_cipher_list(sslContext.native_handle(),
                                     getCyphers(forcedCiphers).c_str()))
    {
        throw boost::beast::system_error{{static_cast<int>(::ERR_get_error()),
                                                 boost::asio::error::get_ssl_category()}};
    }

    if (1 != SSL_CTX_set1_curves_list(sslContext.native_handle(),
                                      "brainpoolP256r1:"
                                      "brainpoolP384r1:"
                                      "prime256v1:"
                                      "secp384r1"))
    {
        throw boost::beast::system_error{{static_cast<int>(::ERR_get_error()),
                                                 boost::asio::error::get_ssl_category()}};
    }

    /* Set allowed signature hash algorithms according to allowed cipher suites above. */
    if (1 != SSL_CTX_set1_sigalgs_list(sslContext.native_handle(),
                                       "ECDSA+SHA384:"
                                       "ECDSA+SHA256:"
                                       "RSA+SHA384:"
                                       "RSA+SHA256"))
    {
        throw boost::beast::system_error{{static_cast<int>(::ERR_get_error()),
                                                 boost::asio::error::get_ssl_category()}};
    }
    // GEMREQ-end A_17124
    // GEMREQ-end A_15751
}

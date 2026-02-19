/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/connection/RootCertificatesMgr.hxx"
#include "shared/util/String.hxx"

#include "shared/util/GLog.hxx"
#include "shared/util/TLog.hxx"

#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/system/error_code.hpp>


void RootCertificatesMgr::loadCaCertificates(boost::asio::ssl::context& context,
                                             const SafeString& caCertificates)
{
    if (caCertificates.size() > 0)
    {
        TVLOG(0) << "loading ca certificates with a total size of " << caCertificates.size();
        try
        {
            context.add_certificate_authority(boost::asio::buffer((const char*)caCertificates, caCertificates.size()));
        }
        catch(const boost::exception& )
        {
            TLOG(WARNING) << "loading ca certificates failed";
            throw;
        }
    }
}

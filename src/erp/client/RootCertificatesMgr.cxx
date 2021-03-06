/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/client/RootCertificatesMgr.hxx"

#include "erp/client/RootCertificates.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/String.hxx"
#include "erp/util/GLog.hxx"

#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/system/error_code.hpp>


void RootCertificatesMgr::loadCaCertificates(boost::asio::ssl::context& context,
                                             const SafeString& caCertificates)
{
    if (caCertificates.size() > 0)
    {
        LOG(INFO) << "loading ca certificates with a total size of " << caCertificates.size();
        try
        {
            context.add_certificate_authority(boost::asio::buffer((const char*)caCertificates, caCertificates.size()));
        }
        catch(const boost::exception& )
        {
            LOG(WARNING) << "loading ca certificates failed";
            throw;
        }
    }
    else
    {
        LOG(INFO) << "loading default root certificates, count of certificates: " << rootCertificates.size();
        for (std::size_t ind = 0; ind < rootCertificates.size(); ind++)
        {
            try
            {
                context.add_certificate_authority(boost::asio::buffer(rootCertificates[ind]));
            }
            catch(const boost::exception& )
            {
                LOG(WARNING) << "loading ca certificates failed at index " << ind;
                throw;
            }
        }
    }
}

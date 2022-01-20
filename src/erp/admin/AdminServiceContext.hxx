/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVICECONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVICECONTEXT_HXX

#include <boost/asio.hpp>

class AdminServer;
class AdminServiceContext
{
public:
    void setIoContext(boost::asio::io_context* ioContext);
    boost::asio::io_context* getIoContext() const;

private:
    boost::asio::io_context* mIoContext{};
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_ADMIN_ADMINSERVICECONTEXT_HXX

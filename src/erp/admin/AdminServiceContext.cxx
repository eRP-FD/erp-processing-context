/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "AdminServiceContext.hxx"

void AdminServiceContext::setIoContext(boost::asio::io_context* ioContext)
{
    mIoContext = ioContext;
}

boost::asio::io_context* AdminServiceContext::getIoContext() const
{
    return mIoContext;
}

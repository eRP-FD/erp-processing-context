/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_SESSION_SYNCHRONOUSSERVERSESSION_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_SESSION_SYNCHRONOUSSERVERSESSION_HXX

#include "erp/server/session/ServerSession.hxx"


/**
 * A synchronous implementation that does not support keeping a connection alive from one request to the next.
 */
class SynchronousServerSession
    : public ServerSession,
      public std::enable_shared_from_this<SynchronousServerSession>
{
public:
    using ServerSession::ServerSession;

    virtual void run (void) override;

private:
    void onTlsHandshakeComplete (boost::beast::error_code ec);
    void do_read (void);
    void on_write (
        bool close,
        boost::beast::error_code ec,
        std::size_t bytes_transferred);
    void do_close (void);
    void on_shutdown (boost::beast::error_code ec);
};

#endif

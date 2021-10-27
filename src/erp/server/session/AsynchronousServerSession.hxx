/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_SESSION_ASYNCHRONOUSSERVERSESSION_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_SESSION_ASYNCHRONOUSSERVERSESSION_HXX

#include "erp/server/session/ServerSession.hxx"

#include <functional>

/**
 * An asynchronous implementation of the ServerSession which supports keep alive.
 * Reading requests and writing responses is done asynchronously.
 */
class AsynchronousServerSession
    : public ServerSession,
      public std::enable_shared_from_this<AsynchronousServerSession>
{
public:
    class SessionData;
    using SessionDataPointer = std::shared_ptr<SessionData>;

    using ServerSession::ServerSession;

    virtual void run (void) override;

private:
    void onTlsHandshakeComplete (boost::beast::error_code ec);
    void do_read ();
    void on_read (ServerRequest request, SessionDataPointer&& data);
    void do_handleRequest (ServerRequest request, SessionDataPointer&& data);
    void do_write (ServerResponse response, const bool keepConnectionAlive, SessionDataPointer&& data);
    void on_write (const bool keepConnectionAlive, SessionDataPointer&& data) noexcept;
    void do_close (SessionDataPointer&& data);
    void on_shutdown (boost::beast::error_code ec, SessionDataPointer&& data);
    template<typename HandlerT, typename ClassT, typename RetT, typename... ArgTs>
    inline std::function<RetT(ArgTs...)> try_handler_detail(HandlerT&& handler, RetT (ClassT::*)(ArgTs...));

    template<typename HandlerT>
    auto try_handler(HandlerT&& handler)
    {
        return try_handler_detail(std::forward<HandlerT>(handler), &HandlerT::operator());
    }
};

#endif

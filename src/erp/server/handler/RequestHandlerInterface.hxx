#ifndef ERP_PROCESSING_CONTEXT_SERVER_HANDLER_REQUESTHANDLERINTERFACE_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_HANDLER_REQUESTHANDLERINTERFACE_HXX

#include "erp/service/Operation.hxx"

#include <string_view>


template<class ServiceContextType>
class SessionContext;


template<class ServiceContextType>
class RequestHandlerInterface
{
public:
    virtual ~RequestHandlerInterface (void) = default;

    virtual void preHandleRequestHook(SessionContext<ServiceContextType>&) {};
    virtual void handleRequest (SessionContext<ServiceContextType>& session) = 0;
    [[nodiscard]] virtual bool allowedForProfessionOID(std::string_view professionOid) const = 0;
    virtual Operation getOperation (void) const = 0;
};

template<class ServiceContextType>
class UnconstrainedRequestHandler : public RequestHandlerInterface<ServiceContextType>
{
public:
    [[nodiscard]] bool allowedForProfessionOID (std::string_view) const override {return true;}
};


#endif

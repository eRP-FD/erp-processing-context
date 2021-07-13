#ifndef ERP_PROCESSING_CONTEXT_HEALTHHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_HEALTHHANDLER_HXX

#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"

namespace model
{
class Health;
}

class HealthHandler : public UnconstrainedRequestHandler<PcServiceContext>
{
public:
    void handleRequest(SessionContext<PcServiceContext>& session) override;
    Operation getOperation(void) const override;

private:
    static bool checkPostgres(SessionContext<PcServiceContext>& session, model::Health& health);
    static bool checkHsm(SessionContext<PcServiceContext>& session, model::Health& health);
    static bool checkRedis(SessionContext<PcServiceContext>& session, model::Health& health);
    static bool checkBna(SessionContext<PcServiceContext>& session, model::Health& health);
    static bool checkTsl(SessionContext<PcServiceContext>& session, model::Health& health);
    static bool checkIdp(SessionContext<PcServiceContext>& session, model::Health& health);
    static bool checkSeedTimer(SessionContext<PcServiceContext>& session, model::Health& health);
    static bool checkTeeTokenUpdater(SessionContext<PcServiceContext>& session, model::Health& health);

    static void handleException(const std::exception_ptr& exceptionPtr, const std::string_view& checkName,
                                const std::function<void(std::optional<std::string_view>)>& statusSetter);
};


#endif//ERP_PROCESSING_CONTEXT_HEALTHHANDLER_HXX

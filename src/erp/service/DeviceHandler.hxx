#ifndef ERP_PROCESSING_CONTEXT_SERVICE_DEVICEHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_DEVICEHANDLER_HXX


#include "erp/service/ErpRequestHandler.hxx"


class DeviceHandler
    : public ErpUnconstrainedRequestHandler
{
public:
    DeviceHandler();
    void handleRequest (PcSessionContext& session) override;
};


#endif

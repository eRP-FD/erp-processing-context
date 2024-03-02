/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

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

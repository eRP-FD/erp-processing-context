/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEMPUTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEMPUTHANDLER_HXX

#include "erp/service/ErpRequestHandler.hxx"


class ChargeItemPutHandler: public ErpRequestHandler
{
public:
    ChargeItemPutHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest(PcSessionContext& session) override;
};


#endif

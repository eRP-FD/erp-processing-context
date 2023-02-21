/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEM_CHARGEITEMPUTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEM_CHARGEITEMPUTHANDLER_HXX


#include "erp/service/chargeitem/ChargeItemBodyHandlerBase.hxx"


class ChargeItemPutHandler: public ChargeItemBodyHandlerBase
{
public:
    ChargeItemPutHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest(PcSessionContext& session) override;
};


#endif

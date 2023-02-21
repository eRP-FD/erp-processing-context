/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEMDELETEHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEMDELETEHANDLER_HXX

#include "erp/service/chargeitem/ChargeItemHandlerBase.hxx"


class ChargeItemDeleteHandler: public ChargeItemHandlerBase
{
public:
    ChargeItemDeleteHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest(PcSessionContext& session) override;
};


#endif

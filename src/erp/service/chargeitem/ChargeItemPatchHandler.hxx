/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEM_CHARGEITEMPATCHHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEM_CHARGEITEMPATCHHANDLER_HXX


#include "erp/service/chargeitem/ChargeItemBodyHandlerBase.hxx"


class ChargeItemPatchHandler: public ChargeItemBodyHandlerBase
{
public:
    ChargeItemPatchHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest(PcSessionContext& session) override;
};

#endif

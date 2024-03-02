/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEM_CHARGEITEMPATCHHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEM_CHARGEITEMPATCHHANDLER_HXX


#include "erp/service/chargeitem/ChargeItemHandlerBase.hxx"


class ChargeItemPatchHandler: public ChargeItemHandlerBase
{
public:
    ChargeItemPatchHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs);

    void handleRequest(PcSessionContext& session) override;
};

#endif

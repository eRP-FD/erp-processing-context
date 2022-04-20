/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEM_CHARGEITEMBODYHANDLERBASE_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEM_CHARGEITEMBODYHANDLERBASE_HXX


#include "erp/service/chargeitem/ChargeItemHandlerBase.hxx"

#include <optional>

namespace model
{
class Binary;
class Bundle;
class ChargeItem;
}


class ChargeItemBodyHandlerBase : public ChargeItemHandlerBase
{
public:
    ChargeItemBodyHandlerBase(Operation operation, const std::initializer_list<std::string_view>& allowedProfessionOIDs);

protected:
    static std::optional<model::Binary> getDispenseItemBinary(const model::ChargeItem& chargeItem);
    static model::Bundle createAndValidateDispenseItem(const model::Binary& containedBinary, const PcServiceContext& serviceContext);
    static void setDispenseItemReference(model::ChargeItem& chargeItem, const model::Bundle& dispenseItemBundle);
};


#endif
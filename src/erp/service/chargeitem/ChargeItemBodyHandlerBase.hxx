/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEM_CHARGEITEMBODYHANDLERBASE_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_CHARGEITEM_CHARGEITEMBODYHANDLERBASE_HXX


#include "erp/service/chargeitem/ChargeItemHandlerBase.hxx"

#include <optional>

namespace model
{
class AbgabedatenPkvBundle;
class Binary;
class ChargeItem;
}


class ChargeItemBodyHandlerBase : public ChargeItemHandlerBase
{
public:
    ChargeItemBodyHandlerBase(Operation operation, const std::initializer_list<std::string_view>& allowedProfessionOIDs);

protected:
    static std::optional<model::Binary> getDispenseItemBinary(const model::ChargeItem& chargeItem);
    static std::pair<::model::AbgabedatenPkvBundle, std::string> validatedBundleFromSigned(
        const ::model::Binary& containedBinary,
        ::PcServiceContext& serviceContext,
        ::VauErrorCode onError);
};


#endif
/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MOCK_CHARGE_ITEM_TABLE_HXX
#define ERP_PROCESSING_CONTEXT_MOCK_CHARGE_ITEM_TABLE_HXX

#include "erp/database/DatabaseModel.hxx"

#include <list>

class UrlArguments;

class MockChargeItemTable
{
public:
    void storeChargeInformation(const ::db_model::ChargeItem& chargeItem, const ::db_model::HashedKvnr& kvnr);
    void updateChargeInformation(const ::db_model::ChargeItem& chargeItem);

    ::std::vector<::db_model::ChargeItem>
    retrieveAllChargeItemsForInsurant(const ::db_model::HashedKvnr& kvnr,
                                      const ::std::optional<::UrlArguments>& search) const;

    ::db_model::ChargeItem retrieveChargeInformation(const ::model::PrescriptionId& id) const;
    ::db_model::ChargeItem retrieveChargeInformationForUpdate(const ::model::PrescriptionId& id) const;

    void deleteChargeInformation(const ::model::PrescriptionId& id);
    void clearAllChargeInformation(const ::db_model::HashedKvnr& insurant);

    uint64_t countChargeInformationForInsurant(const ::db_model::HashedKvnr& kvnr,
                                               const ::std::optional<::UrlArguments>& search) const;

    bool isBlobUsed(::BlobId blobId) const;

private:
    ::std::list<::std::tuple<::db_model::ChargeItem, ::db_model::HashedKvnr>> mChargeItems;
};


#endif// ERP_PROCESSING_CONTEXT_MOCK_CHARGE_ITEM_TABLE_HXX

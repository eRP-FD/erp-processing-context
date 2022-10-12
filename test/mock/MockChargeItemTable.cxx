/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/database/DatabaseModel.hxx"
#include "erp/util/TLog.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "test/mock/MockChargeItemTable.hxx"
#include "test/mock/MockTaskTable.hxx"

void MockChargeItemTable::storeChargeInformation(const ::db_model::ChargeItem& chargeItem,
                                                 const ::db_model::HashedKvnr& kvnr)
{
    switch(chargeItem.prescriptionId.type())
    {

        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::direkteZuweisung:
            Fail2("MockDatabase::storeChargeInformation(...) called with wrong prescription type: " +
                      chargeItem.prescriptionId.toString(),
                  std::logic_error);
            break;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::direkteZuweisungPkv:
            break;
    }
    mChargeItems.emplace_back(chargeItem, kvnr);
}

void MockChargeItemTable::updateChargeInformation(const ::db_model::ChargeItem& chargeItem)
{
    const auto item = ::std::find_if(::std::begin(mChargeItems), ::std::end(mChargeItems),
                                     [id = chargeItem.prescriptionId](const auto& item) {
                                         return (::std::get<::db_model::ChargeItem>(item).prescriptionId == id);
                                     });

    ErpExpect(item != mChargeItems.end(), ::HttpStatus::NotFound,
              "No such charge item: " + chargeItem.prescriptionId.toString());

    ::std::get<::db_model::ChargeItem>(*item).markingFlag = chargeItem.markingFlag;
    ::std::get<::db_model::ChargeItem>(*item).billingData = chargeItem.billingData;
    ::std::get<::db_model::ChargeItem>(*item).billingDataJson = chargeItem.billingDataJson;
}

::std::vector<db_model::ChargeItem>
MockChargeItemTable::retrieveAllChargeItemsForInsurant(const ::db_model::HashedKvnr& kvnr,
                                                       const ::std::optional<::UrlArguments>& search) const
{
    ::std::vector<db_model::ChargeItem> result;

    const auto testArgs = search ? ::std::make_optional<TestUrlArguments>(*search) : ::std::nullopt;

    for (const auto& entry : mChargeItems)
    {
        const auto& chargeItem = ::std::get<::db_model::ChargeItem>(entry);
        if ((::std::get<::db_model::HashedKvnr>(entry) == kvnr) &&
            (! testArgs || (testArgs->matches("entered-date", ::std::make_optional(chargeItem.enteredDate)) &&
                            testArgs->matches("lastUpdated", ::std::make_optional(chargeItem.lastModified)))))
        {
            result.emplace_back(chargeItem);
        }
    }

    if (search.has_value() && search->pagingArgument().isSet())
    {
        return TestUrlArguments(search.value()).applyPaging(::std::move(result));
    }

    return result;
}

::db_model::ChargeItem MockChargeItemTable::retrieveChargeInformation(const model::PrescriptionId& id) const
{
    const auto item = ::std::find_if(::std::begin(mChargeItems), ::std::end(mChargeItems), [id](const auto& item) {
        return (::std::get<::db_model::ChargeItem>(item).prescriptionId == id);
    });

    ErpExpect(item != mChargeItems.end(), ::HttpStatus::NotFound, "No such charge item: " + id.toString());

    return ::std::get<::db_model::ChargeItem>(*item);
}

::db_model::ChargeItem MockChargeItemTable::retrieveChargeInformationForUpdate(const model::PrescriptionId& id) const
{
    return retrieveChargeInformation(id);
}

void MockChargeItemTable::deleteChargeInformation(const ::model::PrescriptionId& id)
{
    mChargeItems.remove_if([id](const auto& item) {
        return (::std::get<::db_model::ChargeItem>(item).prescriptionId == id);
    });
}

uint64_t MockChargeItemTable::countChargeInformationForInsurant(const ::db_model::HashedKvnr& kvnr,
                                                                const ::std::optional<::UrlArguments>& search) const
{
    const auto testArgs = search ? ::std::make_optional<TestUrlArguments>(*search) : ::std::nullopt;

    return ::std::count_if(::std::begin(mChargeItems), ::std::end(mChargeItems), [kvnr, testArgs](const auto& item) {
        return ((::std::get<::db_model::HashedKvnr>(item) == kvnr) &&
                (! testArgs ||
                 (testArgs->matches("entered-date",
                                    ::std::make_optional(::std::get<::db_model::ChargeItem>(item).enteredDate)) &&
                  testArgs->matches("last_Updated",
                                    ::std::make_optional(::std::get<::db_model::ChargeItem>(item).lastModified)))));
    });
}

void MockChargeItemTable::clearAllChargeInformation(const db_model::HashedKvnr& insurant)
{
    mChargeItems.remove_if([insurant](const auto& item) {
        return (::std::get<::db_model::HashedKvnr>(item) == insurant);
    });
}

bool MockChargeItemTable::isBlobUsed(::BlobId blobId) const
{
    return ::std::any_of(::std::begin(mChargeItems), ::std::end(mChargeItems), [blobId](const auto& item) {
        return (::std::get<::db_model::ChargeItem>(item).blobId == blobId);
    });
}

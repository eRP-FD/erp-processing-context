/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/database/DatabaseModel.hxx"
#include "shared/util/TLog.hxx"
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

    const auto item = ::std::find_if(::std::begin(mChargeItems), ::std::end(mChargeItems), [&chargeItem](const auto& item)
    {
        return (::std::get<::db_model::ChargeItem>(item).prescriptionId == chargeItem.prescriptionId);
    });

    if(item != mChargeItems.end())
    {
        throw pqxx::unique_violation(R"(ERROR:  duplicate key value violates unique constraint "charge_item_pkey")");
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

    ::std::get<::db_model::ChargeItem>(*item).accessCode = chargeItem.accessCode;
    ::std::get<::db_model::ChargeItem>(*item).markingFlags = chargeItem.markingFlags;
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
                            testArgs->matches("_lastUpdated", ::std::make_optional(chargeItem.lastModified)))))
        {
            result.emplace_back(chargeItem);
            result.back().accessCode = {} ; // Access code is not read for "retrieveAllChargeItemsForInsurant" query;
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
    Expect(id.isPkv(), "Attempt to retrieve charge information for non-PKV Prescription.");
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

    return static_cast<uint64_t>(
        ::std::count_if(::std::begin(mChargeItems), ::std::end(mChargeItems), [kvnr, testArgs](const auto& item) {
            return ((::std::get<::db_model::HashedKvnr>(item) == kvnr) &&
                    (! testArgs ||
                     (testArgs->matches("entered-date",
                                        ::std::make_optional(::std::get<::db_model::ChargeItem>(item).enteredDate)) &&
                      testArgs->matches("last_Updated",
                                        ::std::make_optional(::std::get<::db_model::ChargeItem>(item).lastModified)))));
        }));
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

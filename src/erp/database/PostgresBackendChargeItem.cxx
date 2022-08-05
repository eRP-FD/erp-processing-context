/*
* (C) Copyright IBM Deutschland GmbH 2022
* (C) Copyright IBM Corp. 2022
*/

#include "erp/database/PostgresBackendChargeItem.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/database/DatabaseModel.hxx"
#include "erp/database/PostgresBackendHelper.hxx"
#include "erp/model/PrescriptionType.hxx"
#include "erp/util/DurationConsumer.hxx"
#include "erp/util/search/UrlArguments.hxx"

#include <magic_enum.hpp>
#include <pqxx/pqxx>

namespace QueryIndices
{
// clang-format off
static constexpr ::pqxx::row::size_type prescription_type =  0u;
static constexpr ::pqxx::row::size_type prescription_id   =  1u;
static constexpr ::pqxx::row::size_type enterer           =  2u;
static constexpr ::pqxx::row::size_type entered_date      =  3u;
static constexpr ::pqxx::row::size_type last_modified     =  4u;
static constexpr ::pqxx::row::size_type marking_flag      =  5u;
static constexpr ::pqxx::row::size_type blob_id           =  6u;
static constexpr ::pqxx::row::size_type salt              =  7u;
static constexpr ::pqxx::row::size_type access_code       =  8u;
static constexpr ::pqxx::row::size_type kvnr              =  9u;
static constexpr ::pqxx::row::size_type prescription      = 10u;
static constexpr ::pqxx::row::size_type prescription_json = 11u;
static constexpr ::pqxx::row::size_type receipt_xml       = 12u;
static constexpr ::pqxx::row::size_type receipt_json      = 13u;
static constexpr ::pqxx::row::size_type billing_data      = 14u;
static constexpr ::pqxx::row::size_type billing_data_json = 15u;
// clang-format on
}

PostgresBackendChargeItem::PostgresBackendChargeItem()
{
#define QUERY(name, query) mQueries.name = {#name, query};

    QUERY(storeChargeInformation, R"--(
        INSERT INTO erp.charge_item (prescription_type, prescription_id, enterer,
                                     entered_date,      last_modified,   marking_flag,
                                     blob_id,           salt,            access_code,
                                     kvnr,              kvnr_hashed,     prescription,
                                     prescription_json, receipt_xml,     receipt_json,
                                     billing_data,      billing_data_json)
            VALUES ( $1::smallint, $2::bigint,  $3,
                     $4,           $5,          $6,
                     $7,           $8,          $9,
                    $10,           $11,        $12,
                    $13,           $14,        $15,
                    $16,           $17))--")

    QUERY(updateChargeInformation, R"--(
        UPDATE erp.charge_item SET marking_flag      = $3,
                                   last_modified     = NOW(),
                                   billing_data      = $4,
                                   billing_data_json = $5
        WHERE prescription_type = $1::smallint
            AND prescription_id = $2::bigint
        )--")

    QUERY(deleteChargeInformation, R"--(
        DELETE FROM erp.charge_item
        WHERE prescription_type = $1::smallint
            AND prescription_id = $2::bigint
        )--")

    QUERY(clearAllChargeInformation, R"--(
        DELETE FROM erp.charge_item
        WHERE kvnr_hashed = $1
    )--")

    QUERY(retrieveChargeInformation, R"--(
        SELECT prescription_type,                prescription_id,                   enterer,
               EXTRACT(EPOCH FROM entered_date), EXTRACT(EPOCH FROM last_modified), marking_flag,
               blob_id,                          salt,                              access_code,
               kvnr,                             prescription,                      prescription_json,
               receipt_xml,                      receipt_json,                      billing_data,
               billing_data_json
        FROM erp.charge_item
        WHERE prescription_type = $1::smallint
            AND prescription_id = $2::bigint
        )--")

    QUERY(retrieveAllChargeItemsForInsurant, R"--(
        SELECT prescription_type,                prescription_id,                   enterer,
               EXTRACT(EPOCH FROM entered_date), EXTRACT(EPOCH FROM last_modified), marking_flag,
               blob_id,                          salt,                              access_code,
               kvnr,                             prescription,                      prescription_json,
               receipt_xml,                      receipt_json,                      billing_data,
               billing_data_json
        FROM erp.charge_item
        WHERE kvnr_hashed = $1
        )--")

    QUERY(countChargeInformationForInsurant, R"--(
        SELECT COUNT(*)
        FROM erp.charge_item
        WHERE kvnr_hashed = $1
    )--")
#undef QUERY
}

void PostgresBackendChargeItem::storeChargeInformation(::pqxx::work& transaction,
                                                       const ::db_model::ChargeItem& chargeItem,
                                                       const ::db_model::HashedKvnr& hashedKvnr) const
{
    TVLOG(2) << mQueries.storeChargeInformation.query;

    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:storeChargeInformation");

    const auto markingFlag = [&chargeItem]() -> ::std::optional<::db_model::postgres_bytea_view> {
        if (chargeItem.markingFlag)
        {
            return chargeItem.markingFlag->binarystring();
        }

        return std::nullopt;
    }();

    const pqxx::result result = transaction.exec_params0(
        mQueries.storeChargeInformation.query,
        // clang-format off
        static_cast<uint32_t>(chargeItem.prescriptionId.type()), chargeItem.prescriptionId.toDatabaseId(), chargeItem.enterer.binarystring(),
        chargeItem.enteredDate.toXsDateTime(),                   chargeItem.lastModified.toXsDateTime(),   markingFlag,
        chargeItem.blobId,                                       chargeItem.salt.binarystring(),           chargeItem.accessCode.binarystring(),
        chargeItem.kvnr.binarystring(),                          hashedKvnr.binarystring(),                chargeItem.prescription->binarystring(),
        chargeItem.prescriptionJson->binarystring(),             chargeItem.receiptXml->binarystring(),    chargeItem.receiptJson->binarystring(),
        chargeItem.billingData.binarystring(),                   chargeItem.billingDataJson.binarystring()
        // clang-format on
    );
}

void PostgresBackendChargeItem::updateChargeInformation(::pqxx::work& transaction,
                                                        const ::db_model::ChargeItem& chargeItem) const
{
    TVLOG(2) << mQueries.updateChargeInformation.query;

    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:updateChargeInformation");

    const auto markingFlag =
        chargeItem.markingFlag ? ::std::make_optional(chargeItem.markingFlag->binarystring()) : ::std::nullopt;

    transaction.exec_params0(mQueries.updateChargeInformation.query,
                             static_cast<uint32_t>(chargeItem.prescriptionId.type()),
                             chargeItem.prescriptionId.toDatabaseId(), markingFlag,
                             chargeItem.billingData.binarystring(), chargeItem.billingDataJson.binarystring());
}

void PostgresBackendChargeItem::deleteChargeInformation(::pqxx::work& transaction,
                                                        const model::PrescriptionId& id) const
{
    TVLOG(2) << mQueries.deleteChargeInformation.query;

    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer("PostgreSQL:deleteChargeInformation");

    transaction.exec_params0(mQueries.deleteChargeInformation.query, static_cast<uint32_t>(id.type()),
                             id.toDatabaseId());
}

void PostgresBackendChargeItem::clearAllChargeInformation(::pqxx::work& transaction,
                                                          const ::db_model::HashedKvnr& kvnr) const
{
    TVLOG(2) << mQueries.clearAllChargeInformation.query;

    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer("PostgreSQL:clearAllChargeInformation");

    transaction.exec_params0(mQueries.clearAllChargeInformation.query, kvnr.binarystring());
}

::db_model::ChargeItem PostgresBackendChargeItem::retrieveChargeInformation(::pqxx::work& transaction,
                                                                            const ::model::PrescriptionId& id) const
{
    return retrieveChargeInformation(transaction, mQueries.retrieveChargeInformation.query, id);
}

::db_model::ChargeItem
PostgresBackendChargeItem::retrieveChargeInformationForUpdate(::pqxx::work& transaction,
                                                              const ::model::PrescriptionId& id) const
{
    auto query = mQueries.retrieveChargeInformation.query;
    query.append("    FOR UPDATE");

    return retrieveChargeInformation(transaction, query, id);
}

::std::vector<::db_model::ChargeItem> PostgresBackendChargeItem::retrieveAllChargeItemsForInsurant(
    ::pqxx::work& transaction, const ::db_model::HashedKvnr& kvnr, const ::std::optional<UrlArguments>& search) const
{
    auto query = mQueries.retrieveAllChargeItemsForInsurant.query;
    if (search.has_value())
    {
        query.append(search->getSqlExpression(transaction.conn(), "                "));
    }
    TVLOG(2) << query;
    TVLOG(2) << "hashedId = " << kvnr.toHex();
    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveAllChargeItems");

    const auto dbResult = transaction.exec_params(query, kvnr.binarystring());

    TVLOG(2) << "got " << dbResult.size() << " results";
    ::std::vector<::db_model::ChargeItem> result;
    result.reserve(dbResult.size());
    for (const auto& row : dbResult)
    {
        result.emplace_back(chargeItemFromQueryResultRow(row));
    }
    return result;
}

uint64_t PostgresBackendChargeItem::countChargeInformationForInsurant(pqxx::work& transaction,
                                                                      const ::db_model::HashedKvnr& kvnr,
                                                                      const ::std::optional<UrlArguments>& search) const
{
    const auto timerKeepAlive =
        ::DurationConsumer::getCurrent().getTimer("PostgreSQL:countChargeInformationForInsurant");
    return ::PostgresBackendHelper::executeCountQuery(transaction, mQueries.countChargeInformationForInsurant.query,
                                                      kvnr, search, "ChargeItem for insurant");
}

::db_model::ChargeItem PostgresBackendChargeItem::retrieveChargeInformation(::pqxx::work& transaction,
                                                                            const ::std::string& query,
                                                                            const ::model::PrescriptionId& id) const
{
    TVLOG(2) << query;

    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveChargeInformation");

    const auto dbResult =
        transaction.exec_params(query, static_cast<int16_t>(::magic_enum::enum_integer(id.type())), id.toDatabaseId());

    TVLOG(2) << "got " << dbResult.size() << " results";
    ErpExpectWithDiagnostics(dbResult.size() == 1, ::HttpStatus::NotFound, "no such charge item", id.toString());

    return chargeItemFromQueryResultRow(dbResult.front());
}

::db_model::ChargeItem PostgresBackendChargeItem::chargeItemFromQueryResultRow(const ::pqxx::row& row) const
{
    Expect3(row.size() == 16u, "Wrong number of fields in result row", ::std::logic_error);

    const auto prescriptionType = row.at(QueryIndices::prescription_type).as<int32_t>();
    const auto prescriptionId = ::model::PrescriptionId::fromDatabaseId(
        ::magic_enum::enum_cast<::model::PrescriptionType>(prescriptionType).value(),
        row.at(QueryIndices::prescription_id).as<int64_t>());

    auto chargeItem = ::db_model::ChargeItem{prescriptionId};
    chargeItem.enterer = ::db_model::EncryptedBlob{row.at(QueryIndices::enterer).as<::db_model::postgres_bytea>()};
    chargeItem.enteredDate = ::model::Timestamp{row.at(QueryIndices::entered_date).as<double>()};
    chargeItem.lastModified = ::model::Timestamp{row.at(QueryIndices::last_modified).as<double>()};
    if (! row.at(QueryIndices::marking_flag).is_null())
    {
        chargeItem.markingFlag =
            ::db_model::EncryptedBlob{row.at(QueryIndices::marking_flag).as<::db_model::postgres_bytea>()};
    }

    chargeItem.blobId = row.at(QueryIndices::blob_id).as<::BlobId>();

    Expect3(! row.at(QueryIndices::salt).is_null(), "Missing salt data in charge item.", ::std::logic_error);
    chargeItem.salt = ::db_model::Blob{row.at(QueryIndices::salt).as<::db_model::postgres_bytea>()};

    chargeItem.accessCode =
        ::db_model::EncryptedBlob{row.at(QueryIndices::access_code).as<::db_model::postgres_bytea>()};

    chargeItem.kvnr = ::db_model::EncryptedBlob{row.at(QueryIndices::kvnr).as<::db_model::postgres_bytea>()};
    chargeItem.prescription =
        ::db_model::EncryptedBlob{row.at(QueryIndices::prescription).as<::db_model::postgres_bytea>()};
    chargeItem.prescriptionJson =
        ::db_model::EncryptedBlob{row.at(QueryIndices::prescription_json).as<::db_model::postgres_bytea>()};
    chargeItem.receiptXml =
        ::db_model::EncryptedBlob{row.at(QueryIndices::receipt_xml).as<::db_model::postgres_bytea>()};
    chargeItem.receiptJson =
        ::db_model::EncryptedBlob{row.at(QueryIndices::receipt_json).as<::db_model::postgres_bytea>()};
    chargeItem.billingData =
        ::db_model::EncryptedBlob{row.at(QueryIndices::billing_data).as<::db_model::postgres_bytea>()};
    chargeItem.billingDataJson =
        ::db_model::EncryptedBlob{row.at(QueryIndices::billing_data_json).as<::db_model::postgres_bytea>()};

    return chargeItem;
}

/*
* (C) Copyright IBM Deutschland GmbH 2021, 2024
* (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
*/

#include "erp/database/PostgresBackendChargeItem.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/database/DatabaseModel.hxx"
#include "erp/database/PostgresBackendHelper.hxx"
#include "erp/model/PrescriptionType.hxx"
#include "erp/util/DurationConsumer.hxx"
#include "erp/util/search/UrlArguments.hxx"

#include <magic_enum/magic_enum.hpp>
#include <pqxx/pqxx>

namespace QueryIndices
{
// clang-format off
    enum ColumnName : ::pqxx::row::size_type {
        prescription_type,
        prescription_id,
        enterer,
        entered_date,
        last_modified,
        marking_flag,
        blob_id,
        salt,
        access_code,
        kvnr,
        prescription,
        prescription_json,
        receipt_xml,
        receipt_json,
        billing_data,
        billing_data_json
    };
// clang-format on
}

PostgresBackendChargeItem::PostgresBackendChargeItem()
{
#define QUERY(name, query) mQueries.name = {#name, query};

// GEMREQ-start A_22137#query
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
// GEMREQ-end A_22137#query

// GEMREQ-start A_22148#query
    QUERY(updateChargeInformation, R"--(
        UPDATE erp.charge_item SET marking_flag      = $3,
                                   last_modified     = NOW(),
                                   billing_data      = $4,
                                   billing_data_json = $5,
                                   access_code       = $6
        WHERE prescription_type = $1::smallint
            AND prescription_id = $2::bigint
        )--")
// GEMREQ-end A_22148#query

// GEMREQ-start A_22117-01#query
    QUERY(deleteChargeInformation, R"--(
        DELETE FROM erp.charge_item
        WHERE prescription_type = $1::smallint
            AND prescription_id = $2::bigint
        )--")
// GEMREQ-end A_22117-01#query

// GEMREQ-start A_22157#query
    QUERY(clearAllChargeInformation, R"--(
        DELETE FROM erp.charge_item
        WHERE kvnr_hashed = $1
    )--")
// GEMREQ-end A_22157#query

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

// GEMREQ-start A_22119#query
    QUERY(retrieveAllChargeItemsForInsurant, R"--(
        SELECT prescription_type,                prescription_id,                   enterer,
               EXTRACT(EPOCH FROM entered_date), EXTRACT(EPOCH FROM last_modified), marking_flag,
               blob_id,                          salt
        FROM erp.charge_item
        WHERE kvnr_hashed = $1
        )--")
// GEMREQ-end A_22119#query

    QUERY(countChargeInformationForInsurant, R"--(
        SELECT COUNT(*)
        FROM erp.charge_item
        WHERE kvnr_hashed = $1
    )--")
#undef QUERY
}

// GEMREQ-start storeChargeInformation
void PostgresBackendChargeItem::storeChargeInformation(::pqxx::work& transaction,
                                                       const ::db_model::ChargeItem& chargeItem,
                                                       const ::db_model::HashedKvnr& hashedKvnr) const
{
    TVLOG(2) << mQueries.storeChargeInformation.query;

    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres,
                                                                        "PostgreSQL:storeChargeInformation");

    const auto markingFlag = [&chargeItem]() -> ::std::optional<::db_model::postgres_bytea_view> {
        if (chargeItem.markingFlags)
        {
            return chargeItem.markingFlags->binarystring();
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
// GEMREQ-end storeChargeInformation

// GEMREQ-start updateChargeInformation
void PostgresBackendChargeItem::updateChargeInformation(::pqxx::work& transaction,
                                                        const ::db_model::ChargeItem& chargeItem) const
{
    TVLOG(2) << mQueries.updateChargeInformation.query;

    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres,
                                                                        "PostgreSQL:updateChargeInformation");

    const auto markingFlags =
        chargeItem.markingFlags ? ::std::make_optional(chargeItem.markingFlags->binarystring()) : ::std::nullopt;

    transaction.exec_params0(mQueries.updateChargeInformation.query,
                             static_cast<uint32_t>(chargeItem.prescriptionId.type()),
                             chargeItem.prescriptionId.toDatabaseId(), markingFlags,
                             chargeItem.billingData.binarystring(), chargeItem.billingDataJson.binarystring(),
                             chargeItem.accessCode.binarystring());
}
// GEMREQ-end updateChargeInformation

// GEMREQ-start A_22117-01#query-call
void PostgresBackendChargeItem::deleteChargeInformation(::pqxx::work& transaction,
                                                        const model::PrescriptionId& id) const
{
    TVLOG(2) << mQueries.deleteChargeInformation.query;

    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres,
                                                                          "PostgreSQL:deleteChargeInformation");

    transaction.exec_params0(mQueries.deleteChargeInformation.query, static_cast<uint32_t>(id.type()),
                             id.toDatabaseId());
}
// GEMREQ-end A_22117-01#query-call

// GEMREQ-start A_22157#query-call
void PostgresBackendChargeItem::clearAllChargeInformation(::pqxx::work& transaction,
                                                          const ::db_model::HashedKvnr& kvnr) const
{
    TVLOG(2) << mQueries.clearAllChargeInformation.query;

    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres,
                                                                          "PostgreSQL:clearAllChargeInformation");

    transaction.exec_params0(mQueries.clearAllChargeInformation.query, kvnr.binarystring());
}
// GEMREQ-end A_22157#query-call

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

// GEMREQ-start A_22119#query-call
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
    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres,
                                                                          "PostgreSQL:retrieveAllChargeItems");

    const auto dbResult = transaction.exec_params(query, kvnr.binarystring());

    TVLOG(2) << "got " << dbResult.size() << " results";
    ::std::vector<::db_model::ChargeItem> result;
    result.reserve(gsl::narrow<size_t>(dbResult.size()));
    for (const auto& row : dbResult)
    {
        result.emplace_back(chargeItemFromQueryResultRow(row, true));
    }
    return result;
}
// GEMREQ-end A_22119#query-call

uint64_t PostgresBackendChargeItem::countChargeInformationForInsurant(pqxx::work& transaction,
                                                                      const ::db_model::HashedKvnr& kvnr,
                                                                      const ::std::optional<UrlArguments>& search) const
{
    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryPostgres, "PostgreSQL:countChargeInformationForInsurant");
    return ::PostgresBackendHelper::executeCountQuery(transaction, mQueries.countChargeInformationForInsurant.query,
                                                      kvnr, search, "ChargeItem for insurant");
}

::db_model::ChargeItem PostgresBackendChargeItem::retrieveChargeInformation(::pqxx::work& transaction,
                                                                            const ::std::string& query,
                                                                            const ::model::PrescriptionId& id) const
{
    TVLOG(2) << query;

    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres,
                                                                          "PostgreSQL:retrieveChargeInformation");

    const auto dbResult =
        transaction.exec_params(query, static_cast<int16_t>(::magic_enum::enum_integer(id.type())), id.toDatabaseId());

    TVLOG(2) << "got " << dbResult.size() << " results";
    ErpExpectWithDiagnostics(dbResult.size() == 1, ::HttpStatus::NotFound, "no such charge item", id.toString());

    return chargeItemFromQueryResultRow(dbResult.front());
}

::db_model::ChargeItem PostgresBackendChargeItem::chargeItemFromQueryResultRow(const ::pqxx::row& row, bool allInsurantChargeItems /* = false */) const
{
    if (allInsurantChargeItems) {
        Expect3(row.size() == 8u, "Wrong number of fields in result row", ::std::logic_error);
    } else {
        Expect3(row.size() == 16u, "Wrong number of fields in result row", ::std::logic_error);
    }

    const auto prescriptionType = gsl::narrow<uint8_t>(row.at(QueryIndices::prescription_type).as<int32_t>());
    const auto prescriptionId = ::model::PrescriptionId::fromDatabaseId(
        ::magic_enum::enum_cast<::model::PrescriptionType>(prescriptionType).value(),
        row.at(QueryIndices::prescription_id).as<int64_t>());

    auto chargeItem = ::db_model::ChargeItem{prescriptionId};
    chargeItem.enteredDate = ::model::Timestamp{row.at(QueryIndices::entered_date).as<double>()};
    chargeItem.lastModified = ::model::Timestamp{row.at(QueryIndices::last_modified).as<double>()};
    if (! row.at(QueryIndices::marking_flag).is_null())
    {
        chargeItem.markingFlags =
            ::db_model::Blob{row.at(QueryIndices::marking_flag).as<::db_model::postgres_bytea>()};
    }

    chargeItem.blobId = row.at(QueryIndices::blob_id).as<::BlobId>();

    Expect3(! row.at(QueryIndices::salt).is_null(), "Missing salt data in charge item.", ::std::logic_error);
    chargeItem.salt = ::db_model::Blob{row.at(QueryIndices::salt).as<::db_model::postgres_bytea>()};

	// When requesting all charge items, skip the decoding of almost all properties because they are not required
	// and thus reduce operating time on the server.
    if (! allInsurantChargeItems)
    {
        chargeItem.enterer = ::db_model::EncryptedBlob{row.at(QueryIndices::enterer).as<::db_model::postgres_bytea>()};
        chargeItem.accessCode = ::db_model::EncryptedBlob{
            row.at(::magic_enum::enum_name(QueryIndices::access_code).data()).as<::db_model::postgres_bytea>()};
        chargeItem.kvnr = ::db_model::EncryptedBlob{
            row.at(::magic_enum::enum_name(QueryIndices::kvnr).data()).as<::db_model::postgres_bytea>()};
        chargeItem.prescription = ::db_model::EncryptedBlob{
            row.at(::magic_enum::enum_name(QueryIndices::prescription).data()).as<::db_model::postgres_bytea>()};
        chargeItem.prescriptionJson = ::db_model::EncryptedBlob{
            row.at(::magic_enum::enum_name(QueryIndices::prescription_json).data()).as<::db_model::postgres_bytea>()};
        chargeItem.receiptXml = ::db_model::EncryptedBlob{
            row.at(::magic_enum::enum_name(QueryIndices::receipt_xml).data()).as<::db_model::postgres_bytea>()};
        chargeItem.receiptJson = ::db_model::EncryptedBlob{
            row.at(::magic_enum::enum_name(QueryIndices::receipt_json).data()).as<::db_model::postgres_bytea>()};
        chargeItem.billingData = ::db_model::EncryptedBlob{
            row.at(::magic_enum::enum_name(QueryIndices::billing_data).data()).as<::db_model::postgres_bytea>()};
        chargeItem.billingDataJson = ::db_model::EncryptedBlob{
            row.at(::magic_enum::enum_name(QueryIndices::billing_data_json).data()).as<::db_model::postgres_bytea>()};
    }
    return chargeItem;
}

/*
* (C) Copyright IBM Deutschland GmbH 2021, 2024
* (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
*/

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_BACKEND_POSTGRES_BACKEND_CHARGE_ITEM_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_BACKEND_POSTGRES_BACKEND_CHARGE_ITEM_HXX

#include "erp/database/DatabaseBackend.hxx"// for fwd declarations and model includes
#include "erp/database/PostgresBackendHelper.hxx"

#include <pqxx/transaction>

class PostgresConnection;

class PostgresBackendChargeItem
{
public:
    PostgresBackendChargeItem();

    void storeChargeInformation(::pqxx::work& transaction, const ::db_model::ChargeItem& chargeItem,
                                const ::db_model::HashedKvnr& hashedKvnr) const;
    void updateChargeInformation(::pqxx::work& transaction, const ::db_model::ChargeItem& chargeItem) const;

    void deleteChargeInformation(::pqxx::work& transaction, const ::model::PrescriptionId& id) const;
    void clearAllChargeInformation(::pqxx::work& transaction, const ::db_model::HashedKvnr& kvnr) const;

    [[nodiscard]] ::db_model::ChargeItem retrieveChargeInformation(::pqxx::work& transaction,
                                                                   const ::model::PrescriptionId& id) const;
    [[nodiscard]] ::db_model::ChargeItem retrieveChargeInformationForUpdate(::pqxx::work& transaction,
                                                                            const ::model::PrescriptionId& id) const;

    [[nodiscard]] ::std::vector<::db_model::ChargeItem>
    retrieveAllChargeItemsForInsurant(::pqxx::work& transaction, const ::db_model::HashedKvnr& kvnr,
                                      const ::std::optional<UrlArguments>& search) const;

    [[nodiscard]] uint64_t countChargeInformationForInsurant(pqxx::work& transaction,
                                                             const ::db_model::HashedKvnr& kvnr,
                                                             const ::std::optional<UrlArguments>& search) const;

private:
    ::db_model::ChargeItem retrieveChargeInformation(::pqxx::work& transaction, const ::std::string& query,
                                                     const ::model::PrescriptionId& id) const;
    [[nodiscard]] ::db_model::ChargeItem chargeItemFromQueryResultRow(const ::pqxx::row& row, bool allInsurantChargeItems = false) const;

    struct Queries {
        QueryDefinition storeChargeInformation;
        QueryDefinition updateChargeInformation;
        QueryDefinition deleteChargeInformation;
        QueryDefinition clearAllChargeInformation;
        QueryDefinition retrieveChargeInformation;
        QueryDefinition retrieveAllChargeItemsForInsurant;
        QueryDefinition countChargeInformationForInsurant;
    };
    Queries mQueries;
};

#endif// ERP_PROCESSING_CONTEXT_SRC_ERP_DATABASE_BACKEND_POSTGRES_BACKEND_CHARGE_ITEM_HXX

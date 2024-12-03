/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TESTURLARGUMENTS_HXX
#define ERP_PROCESSING_CONTEXT_TESTURLARGUMENTS_HXX

#include "erp/database/ErpDatabaseModel.hxx"
#include "erp/model/Communication.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "shared/model/AuditData.hxx"

/**
 * Apply search, sort and paging arguments to the mock database.
 */
class TestUrlArguments
{
public:
    explicit TestUrlArguments (const UrlArguments& mUrlArguments);

    class SearchCommunication : public db_model::Communication
    {
    public:
        explicit SearchCommunication(db_model::Communication comm);
        std::optional<db_model::HashedId> sender;
        std::optional<db_model::HashedId> recipient;
    };
    using Communications = std::vector<SearchCommunication>;
    Communications apply (Communications&& communications) const;
    Communications applySearch (Communications&& communications) const;

    using Tasks = std::vector<db_model::Task>;
    Tasks apply (Tasks&& tasks) const;
    Tasks applySearch (Tasks&& tasks) const;

    using AuditDataContainer = std::vector<db_model::AuditData>;
    AuditDataContainer apply(AuditDataContainer&& auditEvents) const;
    AuditDataContainer applySearch (AuditDataContainer&& auditEvents) const;

    using ChargeItemContainer = std::vector<db_model::ChargeItem>;
    ChargeItemContainer applyPaging (ChargeItemContainer&& chargeItems) const;

    class SearchMedicationDispense : public db_model::MedicationDispense
    {
    public:
        SearchMedicationDispense(db_model::MedicationDispense dbMedicationDispense,
                                 db_model::HashedTelematikId performer,
                                 model::Timestamp whenHandedOver,
                                 std::optional<model::Timestamp> whenPrepared);
        db_model::HashedTelematikId performer;
        model::Timestamp whenHandedOver;
        std::optional<model::Timestamp> whenPrepared;
    };
    using MedicationDispenses = std::vector<SearchMedicationDispense>;
    MedicationDispenses apply(MedicationDispenses&& medicationDispenses) const;
    MedicationDispenses applySearch(MedicationDispenses&& tasks) const;

    /**
     * Return whether the given `value` matches the specified parameter.
     * @return true if the search does not contain an expression for the parameter
     *         false if value
     */
    template<class T>
    bool matches (const std::string& parameterName, const std::optional<T>& value) const;

private:
    const UrlArguments& mUrlArguments;


    Communications applySort (Communications&& communications) const;
    Communications applyPaging (Communications&& communications) const;

    Tasks applySort (Tasks&& tasks) const;
    Tasks applyPaging (Tasks&& tasks) const;

    AuditDataContainer applySort (AuditDataContainer&& auditEvents) const;
    AuditDataContainer applyPaging (AuditDataContainer&& auditEvents) const;

    MedicationDispenses applySort(MedicationDispenses&& tasks) const;
};

extern template bool
TestUrlArguments::matches<db_model::HashedKvnr>(const std::string& parameterName,
                                                const std::optional<db_model::HashedKvnr>& value) const;

extern template bool TestUrlArguments::matches<::db_model::HashedTelematikId>(
    const ::std::string& parameterName, const ::std::optional<::db_model::HashedTelematikId>& value) const;

#endif

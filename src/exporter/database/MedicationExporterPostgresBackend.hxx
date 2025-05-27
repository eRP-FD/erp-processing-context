/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_DATABASE_MEDICATIONEXPORTERPOSTGRESBACKEND_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_DATABASE_MEDICATIONEXPORTERPOSTGRESBACKEND_HXX

#include "exporter/database/MedicationExporterDatabaseBackend.hxx"
#include "shared/database/CommonPostgresBackend.hxx"

#include <string>


class PostgresConnection;
enum class TransactionMode : uint8_t;

class MedicationExporterPostgresBackend : public CommonPostgresBackend, public MedicationExporterDatabaseBackend
{
public:
    MedicationExporterPostgresBackend(TransactionMode mode);
    ~MedicationExporterPostgresBackend() override = default;

    PostgresConnection& connection() const override;

    std::optional<model::EventKvnr> processNextKvnr() override;

    std::vector<db_model::TaskEvent> getAllEventsForKvnr(const model::EventKvnr& kvnr) override;

    void healthCheck() override;
    bool isDeadLetter(const model::EventKvnr& kvnr, const model::PrescriptionId& prescriptionId,
                      model::PrescriptionType prescriptionType) override;
    int markDeadLetter(const model::EventKvnr& kvnr, const model::PrescriptionId& prescriptionId,
                       model::PrescriptionType prescriptionType) override;
    std::optional<db_model::BareTaskEvent> markFirstEventDeadLetter(const model::EventKvnr& kvnr) override;

    void deleteAllEventsForKvnr(const model::EventKvnr& kvnr) override;

    void deleteOneEventForKvnr(const model::EventKvnr& kvnr, db_model::TaskEvent::id_t id) override;

    void updateProcessingDelay(std::int32_t newRetry, std::chrono::seconds delay, const model::EventKvnr& kvnr) override;

    void finalizeKvnr(const model::EventKvnr& kvnr) const override;

    struct TaskEventQueryIndexes {
        pqxx::row::size_type id = 0;
        pqxx::row::size_type prescriptionId = 1;
        pqxx::row::size_type prescriptionType = 2;
        pqxx::row::size_type keyBlobId = 3;
        pqxx::row::size_type salt = 4;
        pqxx::row::size_type kvnr = 5;
        pqxx::row::size_type kvnrHashed = 6;
        pqxx::row::size_type state = 7;
        pqxx::row::size_type usecase = 8;
        pqxx::row::size_type doctorIdentity = 9;
        pqxx::row::size_type pharmacyIdentity = 10;
        pqxx::row::size_type lastModified = 11;
        pqxx::row::size_type authoredOn = 12;
        pqxx::row::size_type healthcareProviderPrescription = 13;
        pqxx::row::size_type medicationDispenseBundleBlobId = 14;
        pqxx::row::size_type medicationDispenseBundleSalt = 15;
        pqxx::row::size_type medicationDispenseBundle = 16;
        pqxx::row::size_type retryCount = 17;
                         int total = 18;
    };

    struct MarkFirsEventDeadletterQueryIndexes {
        pqxx::row::size_type kvnr = 0;
        pqxx::row::size_type kvnrHashed = 1;
        pqxx::row::size_type prescriptionId = 2;
        pqxx::row::size_type prescriptionType = 3;
        pqxx::row::size_type usecase = 4;
        pqxx::row::size_type authoredOn = 5;
        pqxx::row::size_type keyBlobId = 6;
        pqxx::row::size_type salt = 7;
                         int total = 8;
    };

    [[nodiscard]]
    static std::string defaultConnectString();

private:
    /**
     * @brief return value from database field
     *
     * @tparam T target arithmetic type
     * @tparam S source type in DB
     * @param resultRow
     * @param index at result row
     * @param errorText if no value at result row index
     * @return value of T
     */
    template<typename T, typename S = T>
        requires(std::is_arithmetic_v<T>)
    T map(const pqxx::row& resultRow, pqxx::row::size_type index, const std::string& errorText)
    {
        Expect(! resultRow[index].is_null(), errorText);
        return gsl::narrow<T>(resultRow[index].as<S>());
    }

    /**
     * @brief creates an object from database field.
     * Target class must have a constructor with source type.
     *
     * @tparam T target class
     * @tparam S source type in DB
     * @param resultRow
     * @param index at result row
     * @param errorText if no value at result row index
     * @return object of T
     */
    template<typename T, typename S = T>
        requires(not std::is_arithmetic_v<T>)
    T map(const pqxx::row& resultRow, pqxx::row::size_type index, const std::string& errorText)
    {
        Expect(! resultRow[index].is_null(), errorText);
        return T{resultRow[index].as<S>()};
    }

    /**
     * @brief return value from database field
     *
     * @tparam T target arithmetic type
     * @tparam S source type in DB
     * @param resultRow
     * @param index at result row
     * @param errorText if no value at result row index
     * @return value of T
     */
    template<typename T, typename S = T>
        requires(std::is_arithmetic_v<T>)
    T mapDefault(const pqxx::row& resultRow, pqxx::row::size_type index, const S& defaultValue)
    {
        return gsl::narrow<T>(resultRow[index].as<S>(defaultValue));
    }

    /**
     * @brief creates an object from database field.
     * Target class must have a constructor with source type.
     *
     * @tparam T target class
     * @tparam S source type in DB
     * @param resultRow
     * @param index at result row
     * @param errorText if no value at result row index
     * @return object of T
     */
    template<typename T, typename S = T>
        requires(not std::is_arithmetic_v<T>)
    T mapDefault(const pqxx::row& resultRow, pqxx::row::size_type index, const S& defaultValue)
    {
        return T{resultRow[index].as<S>(defaultValue)};
    }

    template<typename T, typename S = T>
        requires(not std::is_arithmetic_v<T>)
    std::optional<T> mapOptional(const pqxx::row& resultRow, pqxx::row::size_type index)
    {
        if (! resultRow[index].is_null())
        {
            return std::make_optional<T>(resultRow[index].as<S>());
        }
        return std::nullopt;
    }
    template<typename T, typename S = T>
        requires(std::is_arithmetic_v<T>)
    std::optional<T> mapOptional(const pqxx::row& resultRow, pqxx::row::size_type index)
    {
        if (! resultRow[index].is_null())
        {
            return std::make_optional<T>(gsl::narrow<T>(resultRow[index].as<S>()));
        }
        return std::nullopt;
    }

    thread_local static PostgresConnection mConnection;
};

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_DATABASE_MEDICATIONEXPORTERPOSTGRESBACKEND_HXX

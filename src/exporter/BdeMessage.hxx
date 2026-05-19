/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_BDEMESSAGE_HXX
#define ERP_PROCESSING_CONTEXT_BDEMESSAGE_HXX

#include "exporter/model/HashedKvnr.hxx"
#include "shared/model/Timestamp.hxx"

#include <optional>

class JsonLog;

class BDEMessage
{
public:
    static constexpr std::string_view log_type{"bde"};

    /// @brief Container for request/response related metadata.
    /// Holds optional metadata collected during request processing, including
    /// timestamps, identifiers, error information, and network-related data.
    /// All fields are optional and may be unset depending on execution path.
    /// NOTE: the lastModified field represents the timestamp of an event from
    /// the database (t-rezeptevent or eventdata entity). It is older than
    /// the startTime, which represents the start of the request.
    struct Data {
        model::Timestamp startTime = model::Timestamp::now();
        std::optional<model::Timestamp> endTime = std::nullopt;
        std::optional<model::Timestamp> lastModified = std::nullopt;
        std::optional<model::HashedKvnr> hashedKvnr = std::nullopt;
        std::optional<std::string> cid = std::nullopt;
        std::optional<std::string> errorCode = std::nullopt;
        std::optional<unsigned int> innerResponseCode = std::nullopt;
        std::optional<std::string> error = std::nullopt;
        std::optional<std::string> host = std::nullopt;
        std::optional<std::string> innerOperation = std::nullopt;
        std::optional<std::string> ip = std::nullopt;
        std::optional<std::string> prescriptionId = std::nullopt;
        std::optional<std::string> processor = std::nullopt;
        std::optional<std::string> requestId = std::nullopt;
        std::optional<unsigned int> responseCode = std::nullopt;
        std::optional<std::string> xContextId = std::nullopt;

        /// @brief Merges another Data object into this one.
        /// Fields which are not set (nullopts) are ignored.
        /// No further checks performed.
        /// @param data Source data to merge from.
        void merge(const Data& data);
    };

    /// @brief Any bde instance must have at least startTime, endTime and lastModified in order
    /// to operate properly. The fields may be overwritten, though.
    explicit BDEMessage(Data data = {.startTime = model::Timestamp::now(),
                                     .endTime = model::Timestamp::now(),
                                     .lastModified = model::Timestamp::now(),
                                     .hashedKvnr = std::nullopt,
                                     .cid = std::nullopt,
                                     .errorCode = std::nullopt,
                                     .innerResponseCode = std::nullopt,
                                     .error = "",
                                     .host = "",
                                     .innerOperation = "",
                                     .ip = "",
                                     .prescriptionId = "",
                                     .processor = "",
                                     .requestId = "",
                                     .responseCode = 0,
                                     .xContextId ="not set"});

    /// @brief Before the objets gets destroyed, it calls the private publish()
    /// method to create the actual log.
    virtual ~BDEMessage();

    /// @brief Merge data into caller objects mData field.
    /// @param data The data to be merged.
    void update(const BDEMessage::Data& data);

    /// @brief Access internal metadata.
    /// @return Current data state of this object.
    const Data& getData() const;

private:
    void publish();

    Data mData;

    std::unique_ptr<JsonLog> mLog;
};

#endif//ERP_PROCESSING_CONTEXT_BDEMESSAGE_HXX

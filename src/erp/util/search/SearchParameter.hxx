/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SEARCHPARAMETER_HXX
#define ERP_PROCESSING_CONTEXT_SEARCHPARAMETER_HXX


#include "shared/model/TimePeriod.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/server/request/ServerRequest.hxx"

#include <pqxx/connection>
#include <variant>


/**
 * A search parameter (opposed to search argument) is used to define which parameters are supported in a search.
  */
class SearchParameter
{
public:

    // These types refer to https://www.hl7.org/fhir/search.html#ptypes not the more general FHIR types.
    // This list is restricted to what is required to handle searching according to gemSpec_FD_eRp.
    // Also the feature set of prefixes and modifies and behavior is restricted to what makes sense given the
    // use cases that are described in gemSpec_FD_eRp.
    enum class Type
    {
        /**
         * See Date description further below, for details. This type is introduced to handle searches
         * where the search value must match the postgresql Date format (YYYY, YYYY-mm, YYYY-mm-dd).
         * Date cannot be used since it appends a time during the render phase.
         */
        SQLDate,

        /**
         * For Date values the prefixes eq, ne, gt, ge, lt, le, sa, eb are supported while ap is not.
         * Target values (values in the database) are treated as time points, not as implicit ranges.
         * Therefore gt and sa are treated as equivalent. Also lt and eb are treated as equivalent.
         */
        Date,

        /**
         * Identical to Date in terms of allowed operations. The difference is how the
         * Date is printed in SQL statements: Instead of passing the datetime format to the
         * database, converts the given datetime to a suuid as used by the database
         * schema (cf. timestamp_from_suuid() and gen_suuid()).
         */
        DateAsUuid,

        /**
         * String values are compared case insensitive(ly?) but otherwise exactly. gemSpec_FD_eRp subjects only KVNRs and TelematikIDs
         * to text search. Therefore whitespace and special characters (which would require UTF-8 interpretation) are not supported.
         *
         * Note that once the implementation for DB encryption is there, these parameters will likely be encrypted, hashed and
         * hex encoded and reduces the valid set of characters even more.
         */
        String,

        /**
         * Enum model::Task::Status, compared using its numerical representation.
         */
        TaskStatus,


       /**
         * hashed Identity value (KVNR/TelematikID)
         */

        HashedIdentity,

        PrescriptionId
    };

    std::string nameUrl;
    std::string nameDb;
    Type type;

    SearchParameter (std::string&& nameUrl, std::string&& nameDb, const Type type);
    SearchParameter (std::string&& nameUrl, const Type type);

    using SearchToDbValue = std::function<std::string_view(const std::string_view&)>;
    std::optional<SearchToDbValue> searchToDbValue;
    SearchParameter (std::string&& nameUrl, std::string&& nameDb, const Type type, SearchToDbValue&& aSearchToDbValue);

    /**
     * For searching parameters by name.
     */
    bool operator< (const SearchParameter& other) const;
};


#endif

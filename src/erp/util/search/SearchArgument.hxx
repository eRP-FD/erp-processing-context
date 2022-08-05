/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SEARCHARGUMENT_HXX
#define ERP_PROCESSING_CONTEXT_SEARCHARGUMENT_HXX


#include "erp/database/DatabaseModel.hxx"
#include "erp/model/TimePeriod.hxx"
#include "fhirtools/model/Timestamp.hxx"
#include "erp/model/Task.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/util/search/SearchParameter.hxx"

#include <variant>
#include <pqxx/connection>


/**
 * A search argument. Extracted from a URL. In the URL it is expected in the form <prefix>:<name>=<value>
 * If prefix is missing, it defaults to "eq". If value is missing the whole argument is ignored. If name does not
 * match a supported parameter, the whole argument is ignored as well.
 */
class SearchArgument
{
public:
    using Type = SearchParameter::Type;

    // The name Prefix was chosen to stay close to the FHIR specification. It represents a comparison operation.
    // If missing from a search argument, then EQ is used as default.
    enum class Prefix
    {
        EQ,    // equal
        NE,    // not equal
        GT,    // greater than
        LT,    // less than
        GE,    // greater than or equal
        LE,    // less than or equal
        SA,    // starts after
        EB     // ends before

        // ap (approximately) is NOT supported (until explicitly requested by Gematik).
    };

    using values_t = std::variant<std::vector<std::optional<model::TimePeriod>>, std::vector<std::string>,
                                  std::vector<model::Task::Status>, std::vector<db_model::HashedId>,
                                  std::vector<model::PrescriptionId>>;

    const std::string name;
    const std::string originalName;
    const values_t values;
    const std::vector<std::string> originalValues;
    const Type type;
    const Prefix prefix;

    SearchArgument (
        Prefix prefix,
        std::string name,
        std::string originalName,
        Type type,
        values_t values,
        std::vector<std::string> originalValues);

    static std::pair<Prefix,std::string> splitPrefixFromValues (const std::string& values, Type type);

    std::string valuesAsString() const;

    size_t valuesCount() const;

    std::string valueAsString (size_t idx) const;
    db_model::HashedId valueAsHashedId(size_t idx) const;
    std::optional<model::TimePeriod> valueAsTimePeriod (size_t idx) const;
    model::Task::Status valueAsTaskStatus (size_t idx) const;
    model::PrescriptionId valueAsPrescriptionId (size_t idx) const;

    std::string prefixAsString (void) const;
    static std::string prefixAsString (const Prefix prefix);

    void appendLinkString (std::ostream& os) const;

private:
    void checkValueIndex(size_t idx) const;

    // these are used for the URL links generated for the self/next/previous links
    std::string dateValuesAsString() const;
    std::string dateValueAsString(size_t idx) const;
    std::string taskStatusAsString() const;
    std::string prescriptionIdValuesAsString() const;

    static std::optional<Prefix> prefixFromString (const std::string& prefixString);
};


#endif

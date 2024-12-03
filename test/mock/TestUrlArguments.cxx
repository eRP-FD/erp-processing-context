/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/mock/TestUrlArguments.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/Task.hxx"
#include "shared/model/Bundle.hxx"

#include <date/date.h>
#include <unordered_set>

namespace {

    template<class T>
    int compareValues (const std::optional<T>& a, const std::optional<T>& b, const SortArgument::Order order)
    {
        if (a.has_value() && b.has_value())
        {
            if (a.value() == b.value())
                return 0;
            else if (a.value() < b.value())
                return order==SortArgument::Order::Increasing ? -1 : +1;
            else
                return order==SortArgument::Order::Increasing ? +1 : -1;
        }
        else if (a.has_value())
        {
            // Null first
            return -1;
        }
        else if (b.has_value())
        {
            // Null first
            return +1;
        }
        else
        {
            return 0;
        }
    }

    bool compareCommunications (
        const TestUrlArguments::SearchCommunication& a,
        const TestUrlArguments::SearchCommunication& b,
        const std::vector<SortArgument>& sortArguments)
    {
        for (const auto& sortArgument : sortArguments)
        {
            int comparisonResult = 0;
            if (sortArgument.nameUrl == "sender")
                comparisonResult = compareValues(a.sender, b.sender, sortArgument.order);
            else if (sortArgument.nameUrl == "recipient")
                comparisonResult = compareValues(a.recipient, b.recipient, sortArgument.order);
            else if (sortArgument.nameUrl == "sent")
                comparisonResult = compareValues(std::make_optional(model::Timestamp::fromDatabaseSUuid(a.id)),
                                                 std::make_optional(model::Timestamp::fromDatabaseSUuid(b.id)),
                                                 sortArgument.order);
            else if (sortArgument.nameUrl == "received")
                comparisonResult = compareValues(a.received, b.received, sortArgument.order);

            if (comparisonResult != 0)
                return comparisonResult < 0;
        }

        // The two elements have proven to be equal regarding all sort arguments. Disambiguate via the communication id.
        return a.id < b.id;
    }

    bool compareTasks(const db_model::Task& taskA,
                      const db_model::Task& taskB,
                      const std::vector<SortArgument>& sortArguments)
    {
        for (const auto& sortArgument : sortArguments)
        {
            int comparisonResult = 0;
            if (sortArgument.nameDb == "status")
            {
                comparisonResult = compareValues(std::optional<model::Task::Status>{taskA.status},
                                                 std::optional<model::Task::Status>{taskB.status},
                                                 sortArgument.order);
            }
            else if (sortArgument.nameDb == "authored_on")
            {
                comparisonResult = compareValues(
                    std::optional<model::Timestamp>{taskA.authoredOn},
                    std::optional<model::Timestamp>{taskB.authoredOn}, sortArgument.order);
            }
            else if (sortArgument.nameDb == "last_modified")
            {
                comparisonResult = compareValues(
                    std::optional<model::Timestamp>{taskA.lastModified},
                    std::optional<model::Timestamp>{taskB.lastModified}, sortArgument.order);
            }
            else if (sortArgument.nameDb == "prescription_id")
            {
                comparisonResult = compareValues(
                    std::optional<std::string>{taskA.prescriptionId.toString()},
                    std::optional<std::string>{taskB.prescriptionId.toString()}, sortArgument.order);
            }

            if (comparisonResult != 0)
                return comparisonResult < 0;
        }

        return taskA.prescriptionId.toDatabaseId() < taskB.prescriptionId.toDatabaseId();
    }

    bool compareAuditEvents(
        const db_model::AuditData& auditEventA,
        const db_model::AuditData& auditEventB,
        const std::vector<SortArgument>& sortArguments)
    {
        for (const auto& sortArgument : sortArguments)
        {
            int comparisonResult = 0;

            if (sortArgument.nameUrl == "date")
            {
                comparisonResult = compareValues(
                    std::optional<model::Timestamp>{auditEventA.recorded},
                    std::optional<model::Timestamp>{auditEventB.recorded}, sortArgument.order);
            }
            else if (sortArgument.nameUrl == "subtype")
            {
                comparisonResult = compareValues(
                    std::optional<std::string_view>(std::string(1, static_cast<char>(auditEventA.action))),
                    std::optional<std::string_view>(std::string(1, static_cast<char>(auditEventB.action))),
                    sortArgument.order);
            }
            if (comparisonResult != 0)
            return comparisonResult < 0;
        }

        return auditEventA.id < auditEventB.id;
    }

    bool compareMedicationDispenses(
        const TestUrlArguments::SearchMedicationDispense& a,
        const TestUrlArguments::SearchMedicationDispense& b,
        const std::vector<SortArgument>& sortArguments)
    {
        for (const auto& sortArgument : sortArguments)
        {
            int comparisonResult = 0;
            if (sortArgument.nameUrl == "whenhandedover")
            {
                comparisonResult = compareValues(
                    std::make_optional(a.whenHandedOver),
                    std::make_optional(b.whenHandedOver),
                    sortArgument.order);
            }
            else if (sortArgument.nameUrl == "whenprepared")
            {
                comparisonResult = compareValues(
                    std::make_optional(a.whenPrepared),
                    std::make_optional(b.whenPrepared),
                    sortArgument.order);
            }
            else if (sortArgument.nameUrl == "performer")
            {
                comparisonResult = compareValues(
                    std::make_optional(a.performer),
                    std::make_optional(b.performer),
                    sortArgument.order);
            }
            if (comparisonResult != 0)
            {
                return comparisonResult < 0;
            }
        }

        // The two elements have proven to be equal regarding all sort arguments. Disambiguate via the prescription id.
        return a.prescriptionId.toDatabaseId() <
               b.prescriptionId.toDatabaseId();
    }

}

TestUrlArguments::SearchCommunication::SearchCommunication(db_model::Communication comm)
    : Communication(std::move(comm))
{}


TestUrlArguments::SearchMedicationDispense::SearchMedicationDispense(
        db_model::MedicationDispense dbMedicationDispense,
        db_model::HashedTelematikId performer,
        model::Timestamp whenHandedOver,
        std::optional<model::Timestamp> whenPrepared)
    : MedicationDispense(std::move(dbMedicationDispense))
    , performer(std::move(performer))
    , whenHandedOver(whenHandedOver)
    , whenPrepared(whenPrepared)
{
}


TestUrlArguments::TestUrlArguments (const UrlArguments& urlArguments)
    : mUrlArguments(urlArguments)
{
}


template<typename T>
bool TestUrlArguments::matches(const std::string& parameterName, const std::optional<T>& value) const //NOLINT(readability-function-cognitive-complexity)
{
    struct Equals {
        bool operator() (const SearchArgument& searchArg, const std::string_view& value)
        {
            for (size_t idx = 0; idx < searchArg.valuesCount(); ++idx)
            {
                if (searchArg.valueAsString(idx) == value)
                {
                    return true;
                }
            }
            return false;
        }
        bool operator() (const SearchArgument& searchArg, const db_model::HashedId& value)
        {
            for (size_t idx = 0; idx < searchArg.valuesCount(); ++idx)
            {
                if (searchArg.valueAsHashedId(idx) == value)
                {
                    return true;
                }
            }
            return false;
        }
        bool operator() (const SearchArgument& searchArg, const model::Task::Status& value)
        {
            for (size_t idx = 0; idx < searchArg.valuesCount(); ++idx)
            {
                if (searchArg.valueAsTaskStatus(idx) == value)
                {
                    return true;
                }
            }
            return false;
        }
    } equals;

    for (const auto& argumentList :
         {std::cref(mUrlArguments.mSearchArguments), std::cref(mUrlArguments.mHiddenSearchArguments)})
    {
        for (const auto& argument : argumentList.get())
        {
            if (argument.originalName == parameterName)
            {
                Expect3(argument.type == SearchParameter::Type::HashedIdentity ||
                            argument.type == SearchParameter::Type::String ||
                            argument.type == SearchParameter::Type::TaskStatus ||
                            argument.type == SearchParameter::Type::PrescriptionId,
                        "Invalid parameter type.", std::logic_error);
                if (! value.has_value())
                {
                    // The search applies to this parameter but there is no value that the search argument could be
                    // matched against => element will be filtered out => return false
                    return false;
                }
                else if (argument.prefix == SearchArgument::Prefix::EQ)
                {
                    return equals(argument, value.value());
                }
                else if (argument.prefix == SearchArgument::Prefix::NE)
                {
                    return not equals(argument, value.value());
                }
                else
                {
                    // Invalid prefix for this argument type => throw to cause error response BadRequest
                    ErpFail(HttpStatus::BadRequest, "invalid prefix for string argument");
                }
            }
        }
    }

    // Parameter was not mentioned by the search arguments => the value does not take part in the search => return true
    return true;
}


template<>
//NOLINTNEXTLINE(readability-function-cognitive-complexity)
bool TestUrlArguments::matches<model::Timestamp> (const std::string& parameterName, const std::optional<model::Timestamp>& value) const
{
    // Default result is true, because if parameter is not mentioned by the search arguments
    // => the value does not take part in the search
    bool result = true;
    for (const auto& argumentList :
         {std::cref(mUrlArguments.mSearchArguments), std::cref(mUrlArguments.mHiddenSearchArguments)})
    {
        for (const auto& argument : argumentList.get())
        {
            if (argument.originalName == parameterName)
            {
                switch (argument.prefix)
                {
                    case SearchArgument::Prefix::EQ: {
                        bool eq = false;
                        for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
                        {
                            const auto& searchValue = argument.valueAsTimePeriod(idx);
                            if (! value.has_value() && ! searchValue.has_value())
                                eq = true;// Match if both value and argument is NULL
                            else if (value.has_value() && searchValue.has_value())
                                // FHIR: The range of the search value fully contains the range of the target value.
                                // B <= T < E
                                eq = searchValue->begin() <= value.value() && value.value() < searchValue->end();
                            // Multiple values of a single parameter are or'd. Search result is true if one value matches.
                            if (eq)
                                break;
                        }
                        result = result && eq;
                        break;
                    }
                    case SearchArgument::Prefix::NE: {
                        bool eq = true;
                        for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
                        {
                            const auto& searchValue = argument.valueAsTimePeriod(idx);
                            if (value.has_value() != searchValue.has_value())
                            {
                                eq = false;
                            }
                            else if (value.has_value() && searchValue.has_value())
                            {
                                // FHIR: The range of the search value does not fully contain the range of the target value.
                                // T < B || E <= T
                                eq = searchValue->begin() <= value.value() && value.value() < searchValue->end();
                            }
                            // Multiple values of a single parameter are or'd. Search result is true if one value does not match.
                            if (! eq)
                                break;
                        }
                        result = result && ! eq;
                        break;
                    }
                    case SearchArgument::Prefix::GT:
                    case SearchArgument::Prefix::SA: {
                        bool gt = false;
                        for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
                        {
                            const auto& searchValue = argument.valueAsTimePeriod(idx);
                            ErpExpect(searchValue.has_value(), HttpStatus::BadRequest,
                                      "unsupported SearchArgument::Prefix");
                            // If there is no value that the search argument could be matched against => no match
                            if (value.has_value())
                                // FHIR: The range above the search value intersects (i.e. overlaps) with the range of the target value.
                                // T >= E
                                // >= instead of > because the upper bound is exclusive
                                // When target values are time points then SA becomes equivalent to GT.
                                gt = value.value() >= searchValue->end();
                            // Multiple values of a single parameter are or'd. Search result is true if one value matches.
                            if (gt)
                                break;
                        }
                        result = result && gt;
                        break;
                    }
                    case SearchArgument::Prefix::LT:
                    case SearchArgument::Prefix::EB: {
                        bool lt = false;
                        for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
                        {
                            const auto& searchValue = argument.valueAsTimePeriod(idx);
                            ErpExpect(searchValue.has_value(), HttpStatus::BadRequest,
                                      "unsupported SearchArgument::Prefix");
                            // If there is no value that the search argument could be matched against => no match
                            if (value.has_value())
                                // FHIR: The range below the search value intersects (i.e. overlaps) with the range of the target value.
                                // T < B
                                // When target values are time points then EB becomes equivalent to LT.
                                lt = value.value() < searchValue->begin();
                            // Multiple values of a single parameter are or'd. Search result is true if one value matches.
                            if (lt)
                                break;
                        }
                        result = result && lt;
                        break;
                    }
                    case SearchArgument::Prefix::GE: {
                        bool ge = false;
                        for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
                        {
                            const auto& searchValue = argument.valueAsTimePeriod(idx);
                            ErpExpect(searchValue.has_value(), HttpStatus::BadRequest,
                                      "unsupported SearchArgument::Prefix");
                            // If there is no value that the search argument could be matched against => no match
                            if (value.has_value())
                                // The range above the search value intersects (i.e. overlaps) with the range of the target value, or the range of the search value fully contains the range of the target value.
                                // T >= B
                                ge = value.value() >= searchValue->begin();
                            // Multiple values of a single parameter are or'd. Search result is true if one value matches.
                            if (ge)
                                break;
                        }
                        result = result && ge;
                        break;
                    }
                    case SearchArgument::Prefix::LE: {
                        bool le = false;
                        for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
                        {
                            const auto& searchValue = argument.valueAsTimePeriod(idx);
                            ErpExpect(searchValue.has_value(), HttpStatus::BadRequest,
                                      "unsupported SearchArgument::Prefix");
                            // If there is no value that the search argument could be matched against => no match
                            if (value.has_value())
                                // The range below the search value intersects (i.e. overlaps) with the range of the target value or the range of the search value fully contains the range of the target value.
                                // T < E
                                // < instead of <= because E is exclusive
                                le = value.value() < searchValue->end();
                            // Multiple values of a single parameter are or'd. Search result is true if one value matches.
                            if (le)
                                break;
                        }
                        result = result && le;
                        break;
                    }
                    default:
                        ErpFail(HttpStatus::InternalServerError, "unsupported SearchArgument::Prefix");
                }
                if (! result)
                    return result;// short cut evaluation
            }
        }
    }

    return result;
}

TestUrlArguments::Communications TestUrlArguments::apply (Communications&& initialCommunications) const
{
    return
        applyPaging(
            applySort(
                applySearch(
                    std::move(initialCommunications))));
}

TestUrlArguments::Tasks TestUrlArguments::apply(TestUrlArguments::Tasks&& tasks) const
{
    return applyPaging(
        applySort(
            applySearch(
                std::move(tasks))));
}

TestUrlArguments::MedicationDispenses TestUrlArguments::apply(MedicationDispenses&& initialMedicationDispenses) const
{
    return
        applySort(
            applySearch(
                std::move(initialMedicationDispenses)));
}


TestUrlArguments::Communications TestUrlArguments::applySearch (Communications&& initialCommunications) const
{
    // Apply search arguments.
    if (! mUrlArguments.mSearchArguments.empty() || ! mUrlArguments.mHiddenSearchArguments.empty())
    {
        Communications communications;

        for (auto& communication : initialCommunications)
        {
            if (matches("sender", communication.sender)
                && matches("recipient", communication.recipient)
                && matches("identifier", std::make_optional(model::Timestamp::fromDatabaseSUuid(communication.id)))
                && matches("sent", std::make_optional(model::Timestamp::fromDatabaseSUuid(communication.id)))
                && matches("received", communication.received))
            {
                communications.emplace_back(std::move(communication));
            }
        }

        return communications;
    }
    else
    {
        // Return an unmodified copy.
        return std::move(initialCommunications);
    }
}


TestUrlArguments::Communications TestUrlArguments::applySort (Communications&& communications) const
{
    // Apply sort arguments.
    if ( ! mUrlArguments.mSortArguments.empty())
    {
        // As Communication is a Resource which is not swappable (and does not have to be outside test code) we
        // can not apply std::sort directly to communications. Use an indirection with a vector of indices instead.
        std::vector<size_t> indices;
        indices.reserve(communications.size());
        for (size_t index=0; index<communications.size(); ++index)
            indices.emplace_back(index);

        std::sort(
            indices.begin(),
            indices.end(),
            [&](const size_t& a, const size_t&b){return compareCommunications(communications[a], communications[b], mUrlArguments.mSortArguments);});

        // Use a brute-force approach to avoid swapping or assignment operator in model::Communication.
        Communications sortedCommunications;
        sortedCommunications.reserve(communications.size());
        for (size_t index=0; index<communications.size(); ++index)
            sortedCommunications.emplace_back(std::move(communications[indices[index]]));

        return sortedCommunications;
    }
    else
        return std::move(communications);
}


TestUrlArguments::Communications TestUrlArguments::applyPaging (Communications&& communications) const
{
    const auto countArg = mUrlArguments.mPagingArgument.getCount();
    const size_t offset = mUrlArguments.mPagingArgument.getOffset();
    const ptrdiff_t remaining = gsl::narrow<ptrdiff_t>(communications.size()) - gsl::narrow<ptrdiff_t>(offset);
    size_t count = 0;
    if(remaining > 0)
        count = std::min(size_t(remaining), countArg);

    Communications page;
    page.reserve(count);

    for (size_t index=0; index<count; ++index)
        page.emplace_back(std::move(communications[index + offset]));

    return page;
}


TestUrlArguments::AuditDataContainer TestUrlArguments::apply(TestUrlArguments::AuditDataContainer&& auditData) const
{
    return applyPaging(applySort(applySearch(std::move(auditData))));
}

TestUrlArguments::Tasks TestUrlArguments::applySearch (TestUrlArguments::Tasks&& initialTasks) const
{
    if (! mUrlArguments.mSearchArguments.empty() || ! mUrlArguments.mHiddenSearchArguments.empty())
    {
        Tasks tasks;

        for (auto& theTask : initialTasks)
        {
            if (matches("status", std::optional<model::Task::Status>{theTask.status})
                && matches("authored-on", std::optional<model::Timestamp>{theTask.authoredOn})
                && matches("modified", std::optional<model::Timestamp>{theTask.lastModified})
                && matches("expiry-date", std::optional<model::Timestamp>{theTask.expiryDate})
                && matches("accept-date", std::optional<model::Timestamp>{theTask.acceptDate}))
            {
                tasks.emplace_back(std::move(theTask));
            }
        }

        return tasks;
    }
    else
    {
        // Return an unmodified copy.
        return std::move(initialTasks);
    }
}

TestUrlArguments::Tasks TestUrlArguments::applySort (TestUrlArguments::Tasks&& tasks) const
{
    if ( ! mUrlArguments.mSortArguments.empty())
    {
        // As Communication is a Resource which is not swappable (and does not have to be outside test code) we
        // can not apply std::sort directly to communications. Use an indirection with a vector of indices instead.
        std::vector<size_t> indices;
        indices.reserve(tasks.size());
        for (size_t index=0; index<tasks.size(); ++index)
            indices.emplace_back(index);

        std::sort(
            indices.begin(),
            indices.end(),
            [&](const size_t& a, const size_t&b){return compareTasks(tasks[a], tasks[b], mUrlArguments.mSortArguments);});

        // Use a brute-force approach to avoid swapping or assignment operator in model::Communication.
        Tasks sortedTasks;
        sortedTasks.reserve(tasks.size());
        for (size_t index=0; index<tasks.size(); ++index)
            sortedTasks.emplace_back(std::move(tasks[indices[index]]));

        return sortedTasks;
    }
    else
        return std::move(tasks);
}

TestUrlArguments::Tasks TestUrlArguments::applyPaging (TestUrlArguments::Tasks&& tasks) const
{
    const auto countArg = mUrlArguments.mPagingArgument.getCount();
    const size_t offset = mUrlArguments.mPagingArgument.getOffset();
    const ptrdiff_t remaining = gsl::narrow<ptrdiff_t>(tasks.size()) - gsl::narrow<ptrdiff_t>(offset);
    size_t count = 0;
    if(remaining > 0)
        count = std::min(size_t(remaining), countArg);

    Tasks page;
    page.reserve(count);

    for (size_t index=0; index<count; ++index)
        page.emplace_back(std::move(tasks[index + offset]));

    return page;
}


TestUrlArguments::AuditDataContainer TestUrlArguments::applySearch (TestUrlArguments::AuditDataContainer&& auditEvents) const
{
    if (! mUrlArguments.mSearchArguments.empty() || ! mUrlArguments.mHiddenSearchArguments.empty())
    {
        AuditDataContainer resultAuditEvents;

        for (auto& auditEvent : auditEvents)
        {
            if (matches("date", std::optional<model::Timestamp>{auditEvent.recorded})
                && matches("entity", std::make_optional(std::string{model::resource::naming_system::prescriptionID} +
                                                        "|" + auditEvent.prescriptionId->toString()))
                && matches("subtype", std::optional<std::string_view>(std::string(1, static_cast<char>(auditEvent.action)))))
            {
                resultAuditEvents.emplace_back(std::move(auditEvent));
            }
        }

        return resultAuditEvents;
    }
    else
    {
        // Return an unmodified copy.
        return std::move(auditEvents);
    }
}

TestUrlArguments::AuditDataContainer TestUrlArguments::applySort (TestUrlArguments::AuditDataContainer&& auditEvents) const
{
    if (!mUrlArguments.mSortArguments.empty())
    {
        // As AuditData is a Resource which is not swappable (and does not have to be outside test code) we
        // can not apply std::sort directly to communications. Use an indirection with a vector of indices instead.
        std::vector<size_t> indices;
        indices.reserve(auditEvents.size());
        for (size_t index = 0; index < auditEvents.size(); ++index)
            indices.emplace_back(index);

        std::sort(
            indices.begin(),
            indices.end(),
            [&](const size_t& a, const size_t&b) {
                return compareAuditEvents(auditEvents[a], auditEvents[b], mUrlArguments.mSortArguments); });

        // Use a brute-force approach to avoid swapping or assignment operator in model::AuditData.
        AuditDataContainer sortedAuditEvents;
        sortedAuditEvents.reserve(auditEvents.size());
        for (size_t index = 0; index < auditEvents.size(); ++index)
            sortedAuditEvents.emplace_back(std::move(auditEvents[indices[index]]));

        return sortedAuditEvents;
    }
    else
        return std::move(auditEvents);
}

TestUrlArguments::AuditDataContainer TestUrlArguments::applyPaging (TestUrlArguments::AuditDataContainer&& auditEvents) const
{
    const auto countArg = mUrlArguments.mPagingArgument.getCount();
    const size_t offset = mUrlArguments.mPagingArgument.getOffset();
    const ptrdiff_t remaining = gsl::narrow<ptrdiff_t>(auditEvents.size()) - gsl::narrow<ptrdiff_t>(offset);
    size_t count = 0;
    if(remaining > 0)
        count = std::min(size_t(remaining), countArg);

    AuditDataContainer page;
    page.reserve(count);

    for (size_t index=0; index<count; ++index)
        page.emplace_back(std::move(auditEvents[index + offset]));

    return page;
}


TestUrlArguments::MedicationDispenses TestUrlArguments::applySearch(MedicationDispenses&& initialMedicationDispenses) const
{
    // Apply search arguments.
    if (! mUrlArguments.mSearchArguments.empty() || ! mUrlArguments.mHiddenSearchArguments.empty())
    {
        MedicationDispenses medicationDispenses;

        for (auto& medicationDispense : initialMedicationDispenses)
        {
            if (matches("whenhandedover", std::make_optional(medicationDispense.whenHandedOver)) &&
                matches("whenprepared", medicationDispense.whenPrepared) &&
                matches("performer", std::make_optional(medicationDispense.performer)) &&
                matches("identifier", std::make_optional(std::string{model::resource::naming_system::prescriptionID} +
                                                         "|" + medicationDispense.prescriptionId.toString())))
            {
                medicationDispenses.emplace_back(std::move(medicationDispense));
            }
        }
        return medicationDispenses;
    }
    else
    {
        // Return an unmodified copy.
        return std::move(initialMedicationDispenses);
    }
}


TestUrlArguments::MedicationDispenses TestUrlArguments::applySort(MedicationDispenses&& medicationDispenses) const
{
    // Apply sort arguments.
    if (!mUrlArguments.mSortArguments.empty())
    {
        // As Communication is a Resource which is not swappable (and does not have to be outside test code) we
        // can not apply std::sort directly to medicationDispenses. Use an indirection with a vector of indices instead.
        std::vector<size_t> indices;
        indices.reserve(medicationDispenses.size());
        for (size_t index = 0; index < medicationDispenses.size(); ++index)
            indices.emplace_back(index);

        std::sort(
            indices.begin(),
            indices.end(),
            [&](const size_t& a, const size_t& b) {return compareMedicationDispenses(medicationDispenses[a], medicationDispenses[b], mUrlArguments.mSortArguments); });

        // Use a brute-force approach to avoid swapping or assignment operator in model::Communication.
        MedicationDispenses sortedMedicationDispenses;
        sortedMedicationDispenses.reserve(medicationDispenses.size());
        for (size_t index = 0; index < medicationDispenses.size(); ++index)
            sortedMedicationDispenses.emplace_back(std::move(medicationDispenses[indices[index]]));

        return sortedMedicationDispenses;
    }
    else
        return std::move(medicationDispenses);
}

TestUrlArguments::ChargeItemContainer TestUrlArguments::applyPaging(ChargeItemContainer&& chargeItems) const
{
    const auto countArg = mUrlArguments.mPagingArgument.getCount();
    const size_t offset = mUrlArguments.mPagingArgument.getOffset();
    const ptrdiff_t remaining = gsl::narrow<ptrdiff_t>(chargeItems.size()) - gsl::narrow<ptrdiff_t>(offset);
    size_t count = 0;
    if (remaining > 0)
        count = std::min(size_t(remaining), countArg);

    ChargeItemContainer page;
    page.reserve(count);

    for (size_t index = 0; index < count; ++index)
        page.emplace_back(std::move(chargeItems[index + offset]));

    return page;
}

template bool TestUrlArguments::matches<db_model::HashedKvnr>(const std::string& parameterName,
                                                              const std::optional<db_model::HashedKvnr>& value) const;

template bool TestUrlArguments::matches<::db_model::HashedTelematikId>(
    const ::std::string& parameterName, const ::std::optional<::db_model::HashedTelematikId>& value) const;

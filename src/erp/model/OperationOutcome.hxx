/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_OPERATIONOUTCOME_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_OPERATIONOUTCOME_HXX

#include "erp/model/Resource.hxx"

#include <rapidjson/document.h>

#include <optional>
#include <vector>


namespace model
{

// Reduced version of OperationOutcome resource, contains only functionality currently needed;

class OperationOutcome : public Resource<OperationOutcome>
{
public:
    class Issue
    {
    public:
        enum class Severity  // https://www.hl7.org/fhir/valueset-issue-severity.html
        {
            fatal,
            error,
            warning,
            information
        };
        enum class Type      // https://www.hl7.org/fhir/valueset-issue-type.html
        {
            invalid,
            structure,
            required,
            value,
            invariant,
            security,
            login,
            unknown,
            expired,
            forbidden,
            suppressed,
            processing,
            not_supported,
            duplicate,
            multiple_matches,
            not_found,
            deleted,
            too_long,
            code_invalid,
            extension,
            too_costly,
            business_rule,
            conflict,
            transient,
            lock_error,
            no_store,
            exception,
            timeout,
            incomplete,
            throttled,
            informational
        };

        [[nodiscard]] static std::string typeToString(Type type);
        [[nodiscard]] static Type stringToType(std::string str);

        Severity severity;
        Type code;
        std::optional<std::string> detailsText;
        std::optional<std::string> diagnostics;
        std::vector<std::string> expression;
    };

    explicit OperationOutcome(Issue&& primaryIssue);

    void addIssue(Issue&& issue);

    [[nodiscard]] std::vector<Issue> issues() const;

private:
    friend Resource<OperationOutcome>;
    explicit OperationOutcome(NumberAsStringParserDocument&& jsonTree);
};


bool operator==(const OperationOutcome::Issue& lhs, const OperationOutcome::Issue& rhs);

}


#endif

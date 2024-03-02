/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_AUDITEVENT_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_AUDITEVENT_HXX

#include "erp/model/PrescriptionId.hxx"
#include "erp/model/Resource.hxx"
#include "erp/model/Timestamp.hxx"

#include <rapidjson/document.h>
#include <optional>

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class AuditEvent : public Resource<AuditEvent>
{
public:
    static constexpr auto resourceTypeName = "AuditEvent";

    // Type is fixed to "rest";
    enum class SubType
    {
        create,     // Create a new resource with a server assigned id.
        read,       // Read the current state of the resource.
        update,     // Update an existing resource by its id (or create it if it is new).
        del         // Delete a resource.
    };
    static const std::unordered_map<SubType, std::string_view> SubTypeNames;
    static const std::unordered_map<std::string_view, SubType> SubTypeNamesReverse;

    enum class Action : char
    {
        create = 'C',   // Create a new database object.
        read = 'R',     // Display or print data.
        update = 'U',   // Update data.
        del = 'D'       // Delete items.
    };
    static const std::unordered_map<std::string_view, Action> ActionNamesReverse;

    enum class Outcome : std::int16_t
    {
        success = 0,        // The operation completed successfully (whether with warnings or not).
        minorFailure = 4,   // The action was not successful due to some kind of minor failure (often equivalent to an HTTP 400 response).
        seriousFailure = 8  // The action was not successful due to some kind of unexpected error (often equivalent to an HTTP 500 response).
    };
    static const std::unordered_map<std::string_view, Outcome> OutcomeNamesReverse;

    static const std::unordered_map<Action, SubType> ActionToSubType;
    static const std::unordered_map<std::string_view, std::string_view> SubTypeNameToActionName;

    enum class AgentType
    {
        human = 0,
        machine = 1
    };
    static const std::unordered_map<AgentType, std::pair<std::string_view,std::string_view>> AgentTypeStrings;
    static const std::unordered_map<std::string_view, AgentType> AgentTypeNamesReverse;

    explicit AuditEvent(ResourceVersion::DeGematikErezeptWorkflowR4 profileVersion =
                      model::ResourceVersion::current<ResourceVersion::DeGematikErezeptWorkflowR4>());

    void setId(const std::string_view& id);
    void setLanguage(const std::string_view& language);
    void setTextDiv(const std::string_view& textDiv);
    void setSubTypeCode(const SubType subTypeCode);
    void setAction(const Action action);
    void setRecorded(const model::Timestamp& recorded);
    void setOutcome(const Outcome outcome);
    void setAgentWho(
        const std::string_view& identifierSystem,
        const std::string_view& identifierValue);
    void setAgentName(const std::string_view& agentName);
    void setAgentType(const AgentType agentType);
    void setSourceObserverReference(const std::string_view& reference);
    void setEntityWhatIdentifier(
        const std::optional<std::string_view>& identifierSystem,
        const std::string_view& identifierValue);
    void setEntityWhatReference(const std::string_view& reference);
    void setEntityName(const std::string_view& entityName);
    void setEntityDescription(const std::string_view& entityDescription);

    [[nodiscard]] std::string_view id() const;
    [[nodiscard]] std::string_view language() const;
    [[nodiscard]] std::string_view textDiv() const;
    [[nodiscard]] SubType subTypeCode() const;
    [[nodiscard]] Action action() const;
    [[nodiscard]] model::Timestamp recorded() const;
    [[nodiscard]] Outcome outcome() const;
    [[nodiscard]] std::tuple<std::optional<std::string_view>, std::optional<std::string_view>> agentWho() const;
    [[nodiscard]] std::string_view agentName() const;
    [[nodiscard]] AgentType agentType() const;
    [[nodiscard]] std::string_view sourceObserverReference() const;
    [[nodiscard]] std::tuple<std::optional<std::string_view>, std::optional<std::string_view>> entityWhatIdentifier() const;
    [[nodiscard]] std::string_view entityWhatReference() const;

    [[nodiscard]] std::string_view entityName() const;
    [[nodiscard]] std::string_view entityDescription() const;

private:
    friend Resource<AuditEvent>;
    explicit AuditEvent(NumberAsStringParserDocument&& jsonTree);
};

}

#endif

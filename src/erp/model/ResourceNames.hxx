/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_RESOURCENAMES_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_RESOURCENAMES_HXX

#include "erp/model/CommunicationPayload.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/model/Timestamp.hxx"

#include <memory>
#include <optional>
#include <string>

#include <rapidjson/pointer.h>

namespace model
{
namespace resource
{
namespace naming_system
{
const std::string_view telematicID = "https://gematik.de/fhir/NamingSystem/TelematikID";
const std::string_view gkvKvid10 = "http://fhir.de/NamingSystem/gkv/kvid-10";
const std::string_view accessCode = "https://gematik.de/fhir/NamingSystem/AccessCode";
const std::string_view secret = "https://gematik.de/fhir/NamingSystem/Secret";
const std::string_view prescriptionID = "https://gematik.de/fhir/NamingSystem/PrescriptionID";
}

namespace code_system
{
const std::string_view consentType = "https://gematik.de/fhir/CodeSystem/Consenttype";
}

namespace structure_definition
{
const std::string communicationLocation = "http://prescriptionserver.telematik/Communication/";

const std::string communication = "https://gematik.de/fhir/StructureDefinition/ErxCommunication";
const std::string communicationInfoReq = "https://gematik.de/fhir/StructureDefinition/ErxCommunicationInfoReq";
const std::string communicationReply = "https://gematik.de/fhir/StructureDefinition/ErxCommunicationReply";
const std::string communicationDispReq = "https://gematik.de/fhir/StructureDefinition/ErxCommunicationDispReq";
const std::string communicationRepresentative = "https://gematik.de/fhir/StructureDefinition/ErxCommunicationRepresentative";
const std::string kbcExForLegalBasis = "https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis";
const std::string kbvExErpStatusCoPayment = "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_StatusCoPayment";
const std::string prescriptionItem = "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle";
const std::string dispenseItem = "http://fhir.abda.de/eRezeptAbgabedaten/StructureDefinition/DAV-PKV-PR-ERP-AbgabedatenBundle";
const std::string receipt = "https://gematik.de/fhir/StructureDefinition/ErxReceipt";
const std::string markingFlag = "https://gematik.de/fhir/StructureDefinition/MarkingFlag";

} // namespace structure_definition

namespace elements
{
const std::string separator{"/"};
}

class ElementName
{
public:
    template <typename T>
    static std::string path(T nodeName)
    {
        return node(nodeName);
    }
    template <typename First, typename ... Rest>
    static std::string path(First first, Rest ... rest)
    {
        return path(first) + path(rest ...);
    }
    explicit ElementName(const std::string& name) :
        mName(name)
    {
    }
    [[nodiscard]] operator const std::string& () const
    {
        return mName;
    }
private:
    static std::string node(const std::string& node)
    {
        return elements::separator + node;
    }
    static std::string node(size_t node)
    {
        return elements::separator + std::to_string(node);
    }
    const std::string mName;
};

namespace elements
{
const ElementName about{ "about" };
const ElementName actor{ "actor" };
const ElementName address{ "address" };
const ElementName amount{ "amount" };
const ElementName assigner{ "assigner" };
const ElementName author{ "author" };
const ElementName basedOn{ "basedOn" };
const ElementName category{ "category" };
const ElementName code{ "code" };
const ElementName coding{ "coding" };
const ElementName contained{ "contained" };
const ElementName contact{ "contact" };
const ElementName contentAttachment{ "contentAttachment" };
const ElementName contentReference{ "contentReference" };
const ElementName contentString{ "contentString" };
const ElementName contentType{ "contentType" };
const ElementName creation{ "creation" };
const ElementName data{ "data" };
const ElementName dateTime{ "dateTime" };
const ElementName denominator{ "denominator" };
const ElementName deviceName{ "deviceName" };
const ElementName display{ "display" };
const ElementName end{ "end" };
const ElementName enterer{ "enterer" };
const ElementName enteredDate{ "enteredDate" };
const ElementName entry{ "entry" };
const ElementName extension{ "extension" };
const ElementName family{ "family" };
const ElementName _family{ "_family" };
const ElementName form{ "form" };
const ElementName hash{ "hash" };
const ElementName id{ "id" };
const ElementName identifier{ "identifier" };
const ElementName ingredient{ "ingredient" };
const ElementName language{ "language" };
const ElementName line{ "line" };
const ElementName _line{ "_line" };
const ElementName meta{ "meta" };
const ElementName name{ "name" };
const ElementName numerator{ "numerator" };
const ElementName patient{ "patient" };
const ElementName payload{ "payload" };
const ElementName payor{ "payor" };
const ElementName performer{ "performer" };
const ElementName period{ "period" };
const ElementName prefix{ "prefix" };
const ElementName _prefix{ "_prefix" };
const ElementName profile{ "profile" };
const ElementName qualification{ "qualification" };
const ElementName received{ "received" };
const ElementName recipient{ "recipient" };
const ElementName reference{ "reference" };
const ElementName resource{ "resource" };
const ElementName resourceType{ "resourceType" };
const ElementName sender{ "sender" };
const ElementName sent{ "sent" };
const ElementName serialNumber{ "serialNumber" };
const ElementName size{ "size" };
const ElementName start{ "start" };
const ElementName status{ "status" };
const ElementName subject{ "subject" };
const ElementName supportingInformation{ "supportingInformation" };
const ElementName strength{ "strength" };
const ElementName system{ "system" };
const ElementName telecom{ "telecom" };
const ElementName text{ "text" };
const ElementName title{ "title" };
const ElementName type{ "type" };
const ElementName unit{ "unit" };
const ElementName url{ "url" };
const ElementName use{ "use" };
const ElementName value{ "value" };
const ElementName valueBoolean{ "valueBoolean" };
const ElementName valueCode{ "valueCode" };
const ElementName valueCoding{ "valueCoding" };
const ElementName valueDate{ "valueDate" };
const ElementName valuePeriod{ "valuePeriod" };
const ElementName valueRatio{ "valueRatio" };
const ElementName valueIdentifier{ "valueIdentifier" };
const ElementName valueString{ "valueString" };
const ElementName version{ "version" };
const ElementName whenHandedOver{ "whenHandedOver" };
const ElementName whenPrepared{ "whenPrepared" };

} // namespace resource

} // namespace elements

} // namespace model

#endif

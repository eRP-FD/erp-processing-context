/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/CommunicationPayload.hxx"
#include "erp/model/CommunicationPayloadItem.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"

#include "erp/util/String.hxx"

using namespace model;
using namespace model::resource;
namespace rj = rapidjson;

static const std::string invalidDataMessage = "The data does not conform to that expected by the FHIR profile or is invalid";

CommunicationPayload::CommunicationPayload(const rj::Value* payloadValue) :
    mItems()
{
    if (payloadValue != nullptr)
    {
        ModelExpect(payloadValue->IsArray(), invalidDataMessage + " (payload has invalid format).");

        const auto& array = payloadValue->GetArray();

        for (auto object = array.Begin(); object != array.End(); ++object)
        {
            for (const auto& itemType : CommunicationPayloadItem::types())
            {
                const rj::Value* itemValue = rj::Pointer(ElementName::path("content" + CommunicationPayloadItem::typeToString(itemType))).Get(*object);

                if (itemValue != nullptr)
                {
                    switch(itemType)
                    {
                        case CommunicationPayloadItem::Type::String:
                        {
                            mItems.push_back(std::make_unique<CommunicationPayloadItemString>(CommunicationPayloadItemString(itemValue)));
                            break;
                        }
                        case CommunicationPayloadItem::Type::Attachment:
                        {
                            mItems.push_back(std::make_unique<CommunicationPayloadItemAttachment>(CommunicationPayloadItemAttachment(itemValue)));
                            break;
                        }
                        case CommunicationPayloadItem::Type::Reference:
                        {
                            mItems.push_back(std::make_unique<CommunicationPayloadItemReference>(CommunicationPayloadItemReference(itemValue)));
                            break;
                        }
                        default:
                        {
                            throw std::logic_error("Payload item type enumerator " + std::to_string(static_cast<int>(itemType)) + " not handled");
                        }
                    }
                    break;
                }
            }
        }
    }
}

void CommunicationPayload::verifyLength() const
{
    size_t length = 0;

    // see https://dth01.ibmgcloud.net/jira/browse/ERP-5092
    ModelExpect(mItems.size() == 1, "Exactly one payload item of type contentString is allowed.");

    for (const auto& item : mItems)
    {
        ModelExpect(item->type() == CommunicationPayloadItem::Type::String, "Exactly one payload item of type contentString is allowed.");

        length += item->length();
        ModelExpect(length <= maxPayloadSize, "Payload must not exceed " + std::to_string(maxPayloadSize/1000) + " KB.");
    }
}

void CommunicationPayload::verifyUrls() const
{
    // see https://dth01.ibmgcloud.net/jira/browse/ERP-5092
    ModelExpect(mItems.size() == 1, "Exactly one payload item of type contentString is allowed.");

    // see https://dth01.ibmgcloud.net/jira/browse/ERP-5092
    for (const auto& item : mItems)
    {
        ModelExpect(item->type() == CommunicationPayloadItem::Type::String, "Exactly one payload item of type contentString is allowed.");

        item->verifyUrls();
    }
}

void CommunicationPayload::verifyMimeTypes() const
{
    // see https://dth01.ibmgcloud.net/jira/browse/ERP-5092
    ModelExpect(mItems.size() == 1, "Exactly one payload item of type contentString is allowed.");

    // see https://dth01.ibmgcloud.net/jira/browse/ERP-5092
    for (const auto& item : mItems)
    {
        ModelExpect(item->type() == CommunicationPayloadItem::Type::String, "Exactly one payload item of type contentString is allowed.");

        item->verifyMimeType();
    }
}

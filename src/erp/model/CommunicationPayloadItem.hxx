/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_COMMUNICATIONPAYLOADITEM_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_COMMUNICATIONPAYLOADITEM_HXX

#include "erp/model/Resource.hxx"

#include <vector>

namespace model
{
class CommunicationPayloadItem
{
public:
    enum class Type
    {
        String,
        Attachment,
        Reference
    };
    static const std::string& typeToString(Type type);
    static const std::vector<Type>& types();

    virtual ~CommunicationPayloadItem () = default;

    Type type() const { return mType; }
    const std::string& typeAsString() const;

    size_t length() const { return mLength; }

protected:
    CommunicationPayloadItem(Type type, const rapidjson::Value* value);

    Type   mType;
    size_t mLength;
};

class CommunicationPayloadItemString : public CommunicationPayloadItem
{
public:
    explicit CommunicationPayloadItemString(const rapidjson::Value* value);
};

} // namespace model

#endif

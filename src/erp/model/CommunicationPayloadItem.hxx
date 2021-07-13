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

    virtual void verifyUrls() const {};
    virtual void verifyMimeType() const {};
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

class CommunicationPayloadItemAttachment : public CommunicationPayloadItem
{
public:
    explicit CommunicationPayloadItemAttachment(const rapidjson::Value* value);

    void verifyUrls() const override;
    void verifyMimeType() const override;
private:
    std::string mMimeType;
    std::string mData;
};

class CommunicationPayloadItemReference : public CommunicationPayloadItem
{
public:
    explicit CommunicationPayloadItemReference(const rapidjson::Value* value);

    void verifyUrls() const override;
private:
    std::string mReference;
};

} // namespace model

#endif

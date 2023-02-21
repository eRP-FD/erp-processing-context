/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/mock/MockCommunicationTable.hxx"
#include "test/mock/MockAccountTable.hxx"
#include "test/mock/TestUrlArguments.hxx"

#include <date/date.h>
#include <algorithm>

#include "erp/util/search/UrlArguments.hxx"

MockCommunicationTable::Row::Row(db_model::HashedId initSender,
                                 db_model::HashedId initRecipient,
                                 model::Communication::MessageType initMessageType,
                                 BlobId initSenderBlobId,
                                 db_model::EncryptedBlob initMessageForSender,
                                 BlobId initRecipientBlobId,
                                 db_model::EncryptedBlob initMessageForRecipient)
    : sender{std::move(initSender)}
    , recipient{std::move(initRecipient)}
    , messageType{initMessageType}
    , senderBlobId{initSenderBlobId}
    , messageForSender{std::move(initMessageForSender)}
    , recipientBlobId{initRecipientBlobId}
    , messageForRecipient{std::move(initMessageForRecipient)}
{
}

MockCommunicationTable::MockCommunicationTable(MockAccountTable& accountTable)
    : mAccounts{accountTable}
{}

std::optional<Uuid> MockCommunicationTable::insertCommunication(
    const model::PrescriptionId& prescriptionId,
    const model::Timestamp& timeSent,
    const model::Communication::MessageType messageType,
    const db_model::HashedId& sender,
    const db_model::HashedId& recipient,
    BlobId senderBlobId,
    const db_model::EncryptedBlob& messageForSender,
    BlobId recipientBlobId,
    const db_model::EncryptedBlob& messageForRecipient)
{
    std::lock_guard lock(mutex);

    // generate a "uuid" sorted by sent time, to emulate the behavior on the postgres DB
    auto dateEnc = date::format("%Y%m%d-%H%M-%S", timeSent.toChronoTimePoint());
    auto uuid = Uuid().toString();
    Uuid communicationId(uuid.replace(0, 16, dateEnc.substr(0, 16)));
    auto [newRow, inserted] = mCommunications.try_emplace(communicationId, sender, recipient, messageType, senderBlobId,
                                                          messageForSender, recipientBlobId, messageForRecipient);
    Expect3(inserted, "Failed to insert new Communication.", std::logic_error);
    newRow->second.prescriptionId = prescriptionId.toDatabaseId();
    return communicationId;
}

uint64_t MockCommunicationTable::countRepresentativeCommunications(const db_model::HashedId& insurantA,
                                                                   const db_model::HashedId& insurantB,
                                                                   const model::PrescriptionId& prescriptionId)
{
    std::shared_lock lock(mutex);
    using model::Communication;
    return static_cast<uint64_t>(
        std::count_if(mCommunications.begin(), mCommunications.end(), [&](const auto& rowPair) {
            const Row& row = rowPair.second;
            return (Communication::MessageType::Representative == row.messageType) &&
                   (prescriptionId.toDatabaseId() == row.prescriptionId) && (row.sender != row.recipient) &&
                   ((insurantA == row.sender && insurantB == row.recipient) ||
                    (insurantA == row.recipient && insurantB == row.sender));
        }));
}


bool MockCommunicationTable::existCommunication(const Uuid& communicationId)
{
    return mCommunications.find(communicationId) != mCommunications.end();
}

std::vector<db_model::Communication>
MockCommunicationTable::retrieveCommunications(const db_model::HashedId& user,
                                               const std::optional<Uuid>& communicationId,
                                               const std::optional<UrlArguments>& search,
                                               bool applyOnlySearch)
{
    std::lock_guard lock(mutex);

    TestUrlArguments::Communications searchable;
    searchable.reserve(mCommunications.size());
    for (auto&& [uuid, communication] : mCommunications)
    {
        if (user == communication.recipient && (! communicationId || communicationId == uuid))
        {
            auto& s = searchable.emplace_back(select(uuid, communication,
                                           {id, received, message_for_recipient, recipient_blob_id}));
            s.recipient = communication.recipient;
            s.sender = communication.sender;
        }
        if (user == communication.sender && (!communicationId || communicationId == uuid))
        {
            auto& s = searchable.emplace_back(select(uuid, communication,
                                           {id, received, message_for_sender, sender_blob_id}));
            s.recipient = communication.recipient;
            s.sender = communication.sender;
        }
    }
    if (search.has_value())
    {
        TestUrlArguments::Communications tmp;
        tmp.swap(searchable);
        if(applyOnlySearch)
            searchable = TestUrlArguments(search.value()).applySearch(std::move(tmp));
        else
            searchable = TestUrlArguments(search.value()).apply(std::move(tmp));
    }
    std::vector<db_model::Communication> result;
    result.reserve(searchable.size());
    for (auto&& comm : searchable) { result.emplace_back(std::move(comm)); }
    return result;
}


uint64_t MockCommunicationTable::countCommunications(const db_model::HashedId& user,
                                                     const std::optional<UrlArguments>& search)
{
    const auto communications = retrieveCommunications(user, {}, search, true/*applyOnlySearch*/);
    return communications.size();
}


std::vector<Uuid> MockCommunicationTable::retrieveCommunicationIds(const db_model::HashedId& recipient)
{
    std::lock_guard lock(mutex);

    std::vector<Uuid> result;
    for (const auto& [uuid, communication] : mCommunications)
    {
        if (recipient == communication.recipient)
            result.emplace_back(uuid);
    }

    return result;
}


std::tuple<std::optional<Uuid>, std::optional<model::Timestamp>>
MockCommunicationTable::deleteCommunication(const Uuid& communicationId, const db_model::HashedId& sender)
{
    std::lock_guard lock(mutex);

    const auto communication = mCommunications.find(communicationId);
    if (communication != mCommunications.end())
    {
        if (communication->second.sender == sender)
        {
            auto timeReceived = communication->second.received;
            mCommunications.erase(communication);
            return std::make_tuple(communicationId, timeReceived);
        }
    }
    return {};
}

void MockCommunicationTable::markCommunicationsAsRetrieved(const std::vector<Uuid>& communicationIds,
                                                           const model::Timestamp& retrieved,
                                                           const db_model::HashedId& recipient)
{
    std::lock_guard lock(mutex);

    for (const auto& id : communicationIds)
    {
        const auto communication = mCommunications.find(id);
        if (communication != mCommunications.end())
        {
            Expect(communication->second.recipient == recipient || communication->second.sender == recipient,
                   "invalid id");
            if (! communication->second.received)
                communication->second.received = retrieved;
        }
    }
}

bool MockCommunicationTable::isBlobUsed(BlobId blobId) const
{
    auto hasBlobId = [blobId](const auto& row) {
    return row.second.senderBlobId == blobId ||
           row.second.recipientBlobId == blobId;

    };
    auto blobUser = find_if(mCommunications.begin(), mCommunications.end(), hasBlobId);
    if (blobUser != mCommunications.end())
    {
        auto prescriptionId = model::PrescriptionId::fromDatabaseId(
            model::PrescriptionType::apothekenpflichigeArzneimittel, blobUser->second.prescriptionId);
        TVLOG(0) << "Blob " << blobId << R"( is still in use by an "task":)" << prescriptionId.toString();
        return true;
    }
    return false;

}


void MockCommunicationTable::deleteCommunicationsForTask(const model::PrescriptionId& taskId)
{
    std::lock_guard lock(mutex);
    for (auto it = mCommunications.begin(); it != mCommunications.end();)
    {
        if (it->second.prescriptionId == taskId.toDatabaseId())
        {
            it = mCommunications.erase(it);
        }
        else
            it++;
    }
}


void MockCommunicationTable::deleteChargeItemCommunicationsForKvnr(const db_model::HashedKvnr& hashedKvnr)
{
    std::lock_guard lock(mutex);
    for (auto it = mCommunications.begin(); it != mCommunications.end();)
    {
        if ((hashedKvnr == it->second.sender || hashedKvnr == it->second.recipient) &&
            (model::Communication::MessageType::ChargChangeReq == it->second.messageType ||
             model::Communication::MessageType::ChargChangeReply == it->second.messageType))
        {
            it = mCommunications.erase(it);
        }
        else
        {
            ++it;
        }
    }
}


void MockCommunicationTable::deleteCommunicationsForChargeItem(const model::PrescriptionId& id)
{
    std::lock_guard lock(mutex);
    for (auto it = mCommunications.begin(); it != mCommunications.end();)
    {
        if (it->second.prescriptionId == id.toDatabaseId() &&
            (model::Communication::MessageType::ChargChangeReq == it->second.messageType ||
             model::Communication::MessageType::ChargChangeReply == it->second.messageType))
        {
            it = mCommunications.erase(it);
        }
        else
            it++;
    }
}


db_model::Communication MockCommunicationTable::select(const Uuid& uuid, const Row& row,
                                                       const std::set<FieldName>& fields) const
{
    db_model::Communication comm;
    comm.id = uuid;
    comm.received = fields.count(received) ? row.received : std::nullopt;
    Expect3(fields.count(message_for_sender) != fields.count(message_for_recipient)
        , "message_for_sender and message_for_recipient cannot be selected at the same time.", std::logic_error);
    Expect3(fields.count(sender_blob_id) != fields.count(recipient_blob_id)
        , "sender_blob_id and recipient_blob_id cannot be selected at the same time.", std::logic_error);
    Expect3(fields.count(message_for_sender) == fields.count(sender_blob_id),
        "message_for_sender and sender_blob_id must be selected together", std::logic_error);
    Expect3(fields.count(message_for_recipient) == fields.count(recipient_blob_id),
        "message_for_recipient and recipient_blob_id must be selected together", std::logic_error);
    if (fields.count(sender_blob_id))
    {
        comm.blobId = row.senderBlobId;
        comm.communication = row.messageForSender;
    }
    else if (fields.count(recipient_blob_id))
    {
        comm.blobId = row.recipientBlobId;
        comm.communication = row.messageForRecipient;
    }
    else
    {
        Fail("either message_for_sender/sender_blob_id or "
             "message_for_recipient/recipient_blob_id must be selected");
    }
    const db_model::HashedId& user = fields.count(sender_blob_id)? row.sender : row.recipient;
    auto salt = mAccounts.getSalt(user, db_model::MasterKeyType::communication, comm.blobId);
    Expect(salt.has_value(), "no salt found.");
    comm.salt = *salt;
    return comm;
}

std::optional<db_model::Communication> MockCommunicationTable::select(const Uuid& uuid,
                                                                      const std::set<FieldName>& fields) const
{
    auto comm = mCommunications.find(uuid);
    if (comm == mCommunications.end())
    {
        return std::nullopt;
    }
    return select(uuid, comm->second, fields);
}

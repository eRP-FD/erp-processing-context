/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MOCKCOMMUNICATIONTABLE_HXX
#define ERP_PROCESSING_CONTEXT_MOCKCOMMUNICATIONTABLE_HXX

#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <tuple>
#include <vector>
#include <map>

#include "erp/database/ErpDatabaseModel.hxx"
#include "erp/model/Communication.hxx"
#include "shared/util/Uuid.hxx"

class MockAccountTable;
class UrlArguments;
namespace model
{
class PrescriptionId;
}


class MockCommunicationTable
{
public:
    explicit MockCommunicationTable(MockAccountTable& accountTable);

    std::optional<Uuid>
    insertCommunication(const model::PrescriptionId& prescriptionId,
                        const model::Timestamp& timeSent,
                        const model::Communication::MessageType messageType,
                        const db_model::HashedId& sender,
                        const db_model::HashedId& recipient,
                        BlobId senderBlobId,
                        const db_model::EncryptedBlob& messageForSender,
                        BlobId recipientBlobId,
                        const db_model::EncryptedBlob& messageForRecipient);

    uint64_t countRepresentativeCommunications(const db_model::HashedId& insurantA,
                                               const db_model::HashedId& insurantB,
                                               const model::PrescriptionId& prescriptionId);

    bool existCommunication(const Uuid& communicationId);

    std::vector<db_model::Communication> retrieveCommunications(const db_model::HashedId& user,
                                                                const std::optional<Uuid>& communicationId,
                                                                const std::optional<UrlArguments>& search,
                                                                bool applyOnlySearch = false);
    uint64_t countCommunications(const db_model::HashedId& user,
                                 const std::optional<UrlArguments>& search);

    std::vector<Uuid> retrieveCommunicationIds(const db_model::HashedId& recipient);

    std::tuple<std::optional<Uuid>, std::optional<model::Timestamp>>
    deleteCommunication(const Uuid& communicationId, const db_model::HashedId& sender);
    void deleteCommunicationsForTask(const model::PrescriptionId& taskId);
    void deleteChargeItemCommunicationsForKvnr(const db_model::HashedKvnr& hashedKvnr);
    void deleteCommunicationsForChargeItem(const model::PrescriptionId& id);

    void markCommunicationsAsRetrieved(const std::vector<Uuid>& communicationIds,
                                       const model::Timestamp& retrieved,
                                       const db_model::HashedId& recipient);

    bool isBlobUsed(BlobId blobId) const;

    class Row
    {
    public:
        explicit Row(db_model::HashedId initSender,
                     db_model::HashedId initRecipient,
                     model::Communication::MessageType initMessageType,
                     BlobId senderBlobId,
                     db_model::EncryptedBlob messageForSender,
                     BlobId recipientBlobId,
                     db_model::EncryptedBlob messageForRecipient);
        Uuid id;
        db_model::HashedId sender;
        db_model::HashedId recipient;
        model::Communication::MessageType messageType;
        std::optional<model::Timestamp> received;
        int64_t prescriptionId = 0;
        BlobId senderBlobId;
        db_model::EncryptedBlob messageForSender;
        BlobId recipientBlobId;
        db_model::EncryptedBlob messageForRecipient;
    };

private:
    enum FieldName
    {
        id,
        sender,
        recipient,
        message_type,
        received,
        prescription_id,
        sender_blob_id,
        message_for_sender,
        recipient_blob_id,
        message_for_recipient,
    };
    db_model::Communication select(const Uuid& uuid, const Row& row, const std::set<FieldName>& fields) const;
    std::optional<db_model::Communication> select(const Uuid& uuid, const std::set<FieldName>& fields) const;

    std::shared_mutex mutex;
    MockAccountTable& mAccounts;
    std::map<Uuid, Row> mCommunications;
};


#endif// ERP_PROCESSING_CONTEXT_MOCKCOMMUNICATIONTABLE_HXX

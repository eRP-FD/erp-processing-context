/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_BLOBDATABASE_HXX
#define ERP_PROCESSING_CONTEXT_BLOBDATABASE_HXX

#include "erp/enrolment/EnrolmentData.hxx"
#include "erp/hsm/ErpTypes.hxx"
#include "erp/tpm/PcrSet.hxx"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


/**
 * This is a first simple interface and implementation of the blob database for the enrolment service.
 * In its current form it may not translate well to a Postgres Database.  Therefore changes are likely.
 */
class BlobDatabase
{
public:
    struct Entry
    {
        BlobType type;
        ErpVector name;
        ErpBlob blob;
        std::optional<std::chrono::system_clock::time_point> expiryDateTime;
        std::optional<std::chrono::system_clock::time_point> startDateTime;
        std::optional<std::chrono::system_clock::time_point> endDateTime;
        /// This `id` value will be auto generated when an `Entry` is stored in the blob db.
        BlobId id = std::numeric_limits<uint32_t>::max();

        // The ak name is only expected to be set for type==BlobType::AttestationPublicKey.
        std::optional<ErpArray<TpmObjectNameLength>> metaAkName;
        // The PCR set of TPM registers is only expected to be set for type==BlobType::Quote.
        std::optional<PcrSet> metaPcrSet;
        // The certificate is only expected to be set for type==BlobType::VauSig.
        ::std::optional<::std::string> certificate;

        bool isBlobValid (std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;

        const ErpArray<TpmObjectNameLength>& getAkName (void) const;
        const PcrSet& getPcrSet (void) const;
    };

    virtual ~BlobDatabase (void) = default;

    virtual Entry getBlob (
        BlobType type,
        BlobId id) const = 0;

    /**
     * Return all blobs sorted by id, increasingly.
     * This is intended to be used to rebuild the BlobCache.
     */
    virtual std::vector<Entry> getAllBlobsSortedById (void) const = 0;

    /**
     * Store the given blob together with optional metadata.
     * Does not overwrite an existing entry.
     * Note that both the key and the combination of type and blob.generation must be unique.
     * Returns the id of the new blob.
     */
    virtual BlobId storeBlob (Entry&& entry) = 0;

    virtual void deleteBlob (
        BlobType type,
        const ErpVector& name) = 0;

    virtual std::vector<bool> hasValidBlobsOfType (std::vector<BlobType>&& blobTypes) const = 0;
};


#endif

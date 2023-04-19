/*
 * (C) Copyright IBM Deutschland GmbH 2023
 * (C) Copyright IBM Corp. 2023
 */

#ifndef ERP_PROCESSING_CONTEXT_VSDMKEYBLOBDATABASE_HXX
#define ERP_PROCESSING_CONTEXT_VSDMKEYBLOBDATABASE_HXX

#include "erp/hsm/ErpTypes.hxx"
#include <chrono>

/**
 * Interface for the VSDM Key Blob table.
 */
class VsdmKeyBlobDatabase
{
public:
    struct Entry {
        char operatorId = 0;
        char version = 0;
        std::chrono::system_clock::time_point createdDateTime{};
        ErpBlob blob;
    };

    virtual ~VsdmKeyBlobDatabase() = default;

    virtual Entry getBlob(char operatorId, char version) const = 0;
    virtual std::vector<Entry> getAllBlobs() const = 0;

    /**
     * Store the given blob together with optional metadata.
     * Does not overwrite an existing entry.
     * Note that the combination of operatorId and version must be unique.
     */
    virtual void storeBlob(Entry&& entry) = 0;

    virtual void deleteBlob(char operatorId, char version) = 0;
};


#endif

/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_BLOBDATABASEHELPER_HXX
#define ERP_PROCESSING_CONTEXT_BLOBDATABASEHELPER_HXX


class BlobDatabaseHelper
{
public:
    static void removeUnreferencedBlobs (void);
    static void removeTestVsdmKeyBlobs(char operatorId);
};


#endif

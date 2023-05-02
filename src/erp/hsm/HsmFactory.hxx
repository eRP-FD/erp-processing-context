/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_HSM_HSMFACTORY_HXX
#define ERP_PROCESSING_CONTEXT_HSM_HSMFACTORY_HXX

#include <memory>

class BlobCache;
class HsmClient;
class HsmSession;
class HsmRawSession;


/**
 * Interface of the HSM factory.
 *
 */
class HsmFactory
{
public:
    explicit HsmFactory (
        std::unique_ptr<HsmClient>&& hsmClient,
        std::shared_ptr<BlobCache>&& blobCache);
    virtual ~HsmFactory (void) = default;

    /**
     *  Create a new HsmSession object, connect it to an external HSM server, log on and return it.
     *
     *  Either returns a valid HamSession object or throws an exception.
     */
    virtual std::shared_ptr<HsmRawSession> rawConnect (void) = 0;
    virtual std::unique_ptr<HsmSession> connect (void);

    BlobCache& getBlobCache (void);

protected:
    std::unique_ptr<HsmClient> mHsmClient;
    std::shared_ptr<BlobCache> mBlobCache;
};

extern std::unique_ptr<HsmFactory> createHsmFactory (std::shared_ptr<BlobCache> blobCache);


#endif

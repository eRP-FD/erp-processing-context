#ifndef ERP_PROCESSING_CONTEXT_HSMPOOLSESSIONREMOVER_HXX
#define ERP_PROCESSING_CONTEXT_HSMPOOLSESSIONREMOVER_HXX

#include <functional>
#include <memory>
#include <mutex>

class HsmSession;


/**
 * Allow for an indirection when HsmPoolSession removes itself from the pool to decouple life time of objects
 * the two classes.
 */
class HsmPoolSessionRemover
{
public:
    explicit HsmPoolSessionRemover (std::function<void(std::unique_ptr<HsmSession>&&)>&& remover);

    /**
     * HsmPoolSession is expected to call this method to remove a session from the pool.
     */
    void removeSession (std::unique_ptr<HsmSession>&& session);

    /**
     * When the pool is released, notify the SessionRemover so that it
     * resets its mRemover member and does not call into the HsmPool anymore.
     * Sessions that are released after this point will not be logged out.
     */
    void notifyPoolRelease (void);

private:
    mutable std::mutex mMutex;
    std::function<void(std::unique_ptr<HsmSession>&&)> mRemover;
};



#endif
